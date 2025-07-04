import React, { useEffect, useState, useMemo } from "react";
import ReactFlow, { useReactFlow, ReactFlowProvider } from "reactflow";
import ELK from "elkjs/lib/elk.bundled.js";
import "reactflow/dist/style.css";

const elk = new ELK();
const nodeSize = { width: 140, height: 90 };

function extractVariables(tree) {
  const vars = [];
  const seen = new Set();

  (function walk(node) {
    if (!node) return;
    if (typeof node === "string") {
      if (!seen.has(node)) {
        seen.add(node);
        vars.push(node);
      }
    } else if (node.type === "VAR") {
      if (!seen.has(node.value)) {
        seen.add(node.value);
        vars.push(node.value);
      }
    } else if (node.type === "NOT") {
      walk(node.child);
    } else {
      walk(node.left);
      walk(node.right);
    }
  })(tree);

  return vars;
}

function treeToElkGraph(tree, id = { current: 0 }) {
  const nodes = [];
  const edges = [];

  function walk(node, parentId = null) {
    const thisId = `n${id.current++}`;
    const isLeaf = typeof node === "string" || node.type === "VAR";
    const label = isLeaf
      ? typeof node === "string"
        ? node
        : node.value
      : node.type;

    nodes.push({
      id: thisId,
      width: nodeSize.width,
      height: nodeSize.height,
      label,
      nodeType: isLeaf ? "leaf" : "internal",
    });

    if (parentId !== null) {
      edges.push({
        id: `${parentId}-${thisId}`,
        sources: [parentId],
        targets: [thisId],
      });
    }

    if (!isLeaf) {
      if (node.type === "NOT") walk(node.child, thisId);
      else {
        walk(node.left, thisId);
        walk(node.right, thisId);
      }
    }
  }

  walk(tree);
  return { nodes, edges };
}

function GraphInner({ tree, wasm, n, onEvaluate, onTruthTable }) {
  const { fitView } = useReactFlow();
  const [baseNodes, setBaseNodes] = useState([]);
  const [styledNodes, setStyledNodes] = useState([]);
  const [edges, setEdges] = useState([]);
  const [varStates, setVarStates] = useState({});

  const variables = useMemo(() => extractVariables(tree), [tree]);

  useEffect(() => {
    if (!tree || !wasm) return;

    const initialVars = Object.fromEntries(variables.map((v) => [v, true]));
    setVarStates(initialVars);

    (async () => {
      const { nodes: rawNodes, edges: rawEdges } = treeToElkGraph(tree);
      const layout = await elk.layout({
        id: "root",
        layoutOptions: {
          "elk.algorithm": "layered",
          "elk.direction": "DOWN",
          "elk.spacing.nodeNode": "80",
          "elk.layered.spacing.nodeNodeBetweenLayers": "120",
          "elk.layered.spacing.nodeNodeSameLayer": "120",
          "elk.spacing.edgeNode": "20",
          "elk.spacing.edgeEdge": "10",
        },
        children: rawNodes,
        edges: rawEdges,
      });

      setEdges(
        rawEdges.map((e) => ({
          id: e.id,
          source: e.sources[0],
          target: e.targets[0],
          type: "straight",
        })),
      );

      const base = layout.children.map((node) => ({
        id: node.id,
        data: {
          label: node.label,
          isLeaf: node.nodeType === "leaf",
        },
        position: { x: node.x, y: node.y },
        width: node.width,
        height: node.height,
        sourcePosition: node.nodeType === "leaf" ? undefined : "bottom",
        targetPosition: "top",
      }));

      setBaseNodes(base);
      requestAnimationFrame(() => fitView({ padding: 0.3 }));
    })();
  }, [tree, wasm, variables, fitView, n]);

  useEffect(() => {
    if (!baseNodes.length || !wasm) return;

    try {
      const resultRaw = wasm.evaluate_expr_full_json(
        n,
        JSON.stringify(varStates),
      );
      const resultMap = JSON.parse(resultRaw);

      const styled = baseNodes.map((node) => {
        const value = resultMap[node.id];
        return {
          ...node,
          style: {
            backgroundColor:
              value === undefined ? "#eee" : value ? "#d4edda" : "#f8d7da",
            border: "1px solid #888",
            fontSize: "30px",
          },
        };
      });

      setStyledNodes(styled);

      const output = resultMap["n0"] ?? null;
      onEvaluate?.(output ? "true" : "false");
      onTruthTable?.([
        { inputs: { ...varStates }, output: output ? "true" : "false" },
      ]);
    } catch (err) {
      console.error("Eval failed", err);
      onEvaluate?.(null);
      onTruthTable?.([]);
    }
  }, [baseNodes, varStates, wasm, n, onEvaluate, onTruthTable]);

  const toggleVariable = (v) => {
    setVarStates((prev) => ({ ...prev, [v]: !prev[v] }));
  };

  return (
    <div className="w-full h-full flex flex-col">
      <div className="flex-1">
        <ReactFlow
          nodes={styledNodes}
          edges={edges}
          fitView
          panOnScroll
          zoomOnScroll
          minZoom={0.01}
          maxZoom={2}
          className="w-full h-full"
          onNodeClick={(_, node) => {
            const { label, isLeaf } = node.data || {};
            if (isLeaf && label in varStates) toggleVariable(label);
          }}
        />
      </div>
      <div className="bg-[#fdf9ee] border-t border-[#ccc7b7] px-4 py-2 flex flex-wrap gap-2 justify-center">
        {variables.map((v) => (
          <button
            key={v}
            className={`btn text-sm px-3 py-1 ${
              varStates[v] ? "bg-green-100" : "bg-red-100"
            }`}
            onClick={() => toggleVariable(v)}
          >
            {v}: {varStates[v] ? "ON" : "OFF"}
          </button>
        ))}
      </div>
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
