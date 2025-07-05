import React, { useEffect, useState, useMemo, useCallback } from "react";
import ReactFlow, {
  ReactFlowProvider,
  useReactFlow,
  Handle,
  Position,
} from "reactflow";
import ELK from "elkjs/lib/elk.bundled.js";
import "reactflow/dist/style.css";

const elk = new ELK();
const nodeSize = { width: 140, height: 90 };

const DEFAULT_BG = "bg-gray-200";
const TRUE_BG = "bg-logicTrue";
const FALSE_BG = "bg-logicFalse";

function LogicNode({ id, data }) {
  const { label, hasIncoming, hasOutgoing, backgroundClass, width, height } =
    data;
  return (
    <div
      className={`relative flex items-center justify-center ${backgroundClass} text-[30px]`}
      style={{ width, height }}
    >
      {hasIncoming && (
        <Handle
          type="target"
          position={Position.Top}
          id={`${id}-target`}
          className="bg-edge"
        />
      )}
      {label}
      {hasOutgoing && (
        <Handle
          type="source"
          position={Position.Bottom}
          id={`${id}-source`}
          className="bg-edge"
        />
      )}
    </div>
  );
}

const nodeTypes = { logic: LogicNode };

function extractVariables(tree) {
  const vars = new Set();
  const stack = [tree];
  while (stack.length) {
    const n = stack.pop();
    if (!n) continue;
    if (typeof n === "string") {
      vars.add(n);
    } else if (n.type === "VAR") {
      vars.add(n.value);
    } else if (n.type === "NOT") {
      stack.push(n.child);
    } else {
      stack.push(n.left, n.right);
    }
  }

  return Array.from(vars).sort(
    (a, b) => a.length - b.length || a.localeCompare(b),
  );
}

function treeToElkGraph(tree) {
  let counter = 0;
  const nodes = [];
  const edges = [];
  function build(n, parentId) {
    const myId = `n${counter++}`;
    const isLeaf = typeof n === "string" || n.type === "VAR";
    const label = isLeaf ? (typeof n === "string" ? n : n.value) : n.type;
    nodes.push({ id: myId, label });
    if (parentId) {
      edges.push({
        id: `${parentId}-${myId}`,
        sources: [parentId],
        targets: [myId],
      });
    }
    if (!isLeaf) {
      if (n.type === "NOT") {
        build(n.child, myId);
      } else {
        build(n.left, myId);
        build(n.right, myId);
      }
    }
  }
  build(tree, null);
  return { nodes, edges };
}

const ELK_OPTIONS = {
  "elk.algorithm": "layered",
  "elk.direction": "DOWN",
  "elk.spacing.nodeNode": "80",
  "elk.layered.spacing.nodeNodeBetweenLayers": "120",
  "elk.layered.spacing.nodeNodeSameLayer": "120",
  "elk.spacing.edgeNode": "20",
  "elk.spacing.edgeEdge": "10",
};

function GraphInner({ tree, wasm, n, onEvaluate, onTruthTable }) {
  const { fitView } = useReactFlow();

  const [nodesMeta, setNodesMeta] = useState([]);
  const [edges, setEdges] = useState([]);
  const [varStates, setVarStates] = useState({});

  const variables = useMemo(() => extractVariables(tree), [tree]);

  useEffect(() => {
    if (!tree || !wasm) return;
    setNodesMeta([]);
    setEdges([]);
    setVarStates(Object.fromEntries(variables.map((v) => [v, true])));

    let active = true;
    (async () => {
      const { nodes: rawNodes, edges: rawRawEdges } = treeToElkGraph(tree);
      const layout = await elk.layout({
        id: "root",
        layoutOptions: ELK_OPTIONS,
        children: rawNodes.map((n) => ({
          ...n,
          width: nodeSize.width,
          height: nodeSize.height,
        })),
        edges: rawRawEdges,
      });
      if (!active) return;

      setEdges(
        rawRawEdges.map((e) => {
          const [src] = e.sources;
          const [tgt] = e.targets;
          return {
            id: e.id,
            source: src,
            target: tgt,
            type: "straight",
            sourceHandle: `${src}-source`,
            targetHandle: `${tgt}-target`,
            style: {
              strokeWidth: 2,
            },
          };
        }),
      );

      setNodesMeta(
        layout.children.map((n) => {
          const hasIncoming = rawRawEdges.some((e) => e.targets[0] === n.id);
          const hasOutgoing = rawRawEdges.some((e) => e.sources[0] === n.id);
          return {
            id: n.id,
            type: "logic",
            position: { x: n.x, y: n.y },
            width: n.width,
            height: n.height,
            data: {
              label: n.label,
              hasIncoming,
              hasOutgoing,
              backgroundClass: DEFAULT_BG,
              width: n.width,
              height: n.height,
            },
          };
        }),
      );

      requestAnimationFrame(() => fitView({ padding: 0.3 }));
    })();

    return () => {
      active = false;
    };
  }, [tree, wasm, variables, fitView]);

  const nodes = useMemo(() => {
    if (!nodesMeta.length || !wasm) return [];
    let resultMap = {};
    try {
      resultMap = JSON.parse(
        wasm.evaluate_expr_full_json(n, JSON.stringify(varStates)),
      );
    } catch {}
    return nodesMeta.map((node) => {
      const val = resultMap[node.id];
      const bg = val === undefined ? DEFAULT_BG : val ? TRUE_BG : FALSE_BG;
      return {
        ...node,
        data: { ...node.data, backgroundClass: bg },
      };
    });
  }, [nodesMeta, varStates, wasm, n]);

  useEffect(() => {
    if (!nodesMeta.length || !wasm) return;
    let resultMap = {};
    try {
      resultMap = JSON.parse(
        wasm.evaluate_expr_full_json(n, JSON.stringify(varStates)),
      );
    } catch {}
    const out = resultMap["n0"] ?? null;
    onEvaluate?.(out ? "true" : "false");
    onTruthTable?.([
      { inputs: { ...varStates }, output: out ? "true" : "false" },
    ]);
  }, [nodesMeta, varStates, wasm, n, onEvaluate, onTruthTable]);

  const toggleVariable = useCallback(
    (v) => setVarStates((prev) => ({ ...prev, [v]: !prev[v] })),
    [],
  );

  return (
    <div className="layout-graph">
      <div className="flex-1">
        <ReactFlow
          nodes={nodes}
          edges={edges}
          nodeTypes={nodeTypes}
          fitView
          fitViewOptions={{ padding: 0.2 }}
          panOnScroll
          zoomOnScroll
          minZoom={0.01}
          maxZoom={2}
          onNodeClick={(_, node) =>
            node.data.hasIncoming &&
            !node.data.hasOutgoing &&
            toggleVariable(node.data.label)
          }
          className="w-full h-[90%]"
        />
      </div>
      {nodesMeta.length > 0 && (
        <div className="bg-section border-t border-edge px-4 py-2 flex flex-wrap gap-2 justify-center">
          {variables.map((v) => (
            <button
              key={v}
              className={`btn btn-small btn-hover ${varStates[v] ? TRUE_BG : FALSE_BG}`}
              onClick={() => toggleVariable(v)}
            >
              {v}: {varStates[v] ? "ON" : "OFF"}
            </button>
          ))}
        </div>
      )}{" "}
    </div>
  );
}

export default function Graph(props) {
  return (
    <ReactFlowProvider>
      <GraphInner {...props} />
    </ReactFlowProvider>
  );
}
