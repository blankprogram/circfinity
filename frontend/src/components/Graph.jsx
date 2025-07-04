import React, { useEffect, useState, useMemo } from "react";
import ReactFlow, { useReactFlow, ReactFlowProvider } from "reactflow";
import ELK from "elkjs/lib/elk.bundled.js";
import "reactflow/dist/style.css";

const elk = new ELK();
const nodeSize = { width: 140, height: 90 };

function extractVariables(tree, vars = new Set()) {
  if (!tree) return vars;
  if (typeof tree === "string") vars.add(tree);
  else if (tree.type === "VAR") vars.add(tree.value);
  else if (tree.type === "NOT") extractVariables(tree.child, vars);
  else extractVariables(tree.left, vars) && extractVariables(tree.right, vars);
  return vars;
}

function treeToElkGraph(tree, id = { current: 0 }) {
  const nodes = [];
  const edges = [];

  function walk(node, parentId = null) {
    const thisId = `${id.current++}`;
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
      else (walk(node.left, thisId), walk(node.right, thisId));
    }
  }

  walk(tree);
  return { nodes, edges };
}

function getNodeStyle(isLeaf, isOn) {
  return {
    backgroundColor: isLeaf ? (isOn ? "#d4edda" : "#f8d7da") : "#fee",
    border: "1px solid #888",
    fontSize: "30px",
  };
}

async function layoutWithElk(nodes, edges, varStates) {
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
    children: nodes,
    edges,
  });

  return layout.children.map((node) => {
    const isLeaf = node.nodeType === "leaf";
    const isOn = varStates[node.label] ?? true;

    return {
      id: node.id,
      data: { label: node.label, isLeaf },
      position: { x: node.x, y: node.y },
      width: node.width,
      height: node.height,
      style: getNodeStyle(isLeaf, isOn),
      sourcePosition: isLeaf ? undefined : "bottom",
      targetPosition: "top",
    };
  });
}

function GraphInner({ tree }) {
  const { fitView } = useReactFlow();
  const [nodes, setNodes] = useState([]);
  const [edges, setEdges] = useState([]);
  const [varStates, setVarStates] = useState({});

  const variables = useMemo(() => Array.from(extractVariables(tree)), [tree]);

  const layoutAndSetGraph = async (varStates) => {
    const { nodes: rawNodes, edges: rawEdges } = treeToElkGraph(tree);
    const laidOutNodes = await layoutWithElk(rawNodes, rawEdges, varStates);
    setNodes(laidOutNodes);
    setEdges(
      rawEdges.map((e) => ({
        id: e.id,
        source: e.sources[0],
        target: e.targets[0],
        type: "straight",
      })),
    );
    requestAnimationFrame(() => fitView({ padding: 0.3 }));
  };

  useEffect(() => {
    if (!tree) return;
    const initial = Object.fromEntries(variables.map((v) => [v, true]));
    setVarStates(initial);
    layoutAndSetGraph(initial);
  }, [tree, variables, fitView]);

  useEffect(() => {
    if (!tree) return;
    layoutAndSetGraph(varStates);
  }, [varStates, tree]);

  return (
    <div className="relative w-full h-full">
      <ReactFlow
        nodes={nodes}
        edges={edges}
        fitView
        panOnScroll
        zoomOnScroll
        minZoom={0.1}
        maxZoom={2}
        className="w-full h-full"
        onNodeClick={(_, node) => {
          const { label, isLeaf } = node.data || {};
          if (isLeaf && label && label in varStates) {
            setVarStates((prev) => ({
              ...prev,
              [label]: !prev[label],
            }));
          }
        }}
      />
      <div className="absolute bottom-0 left-0 right-0 bg-[#fdf9ee] border-t border-[#ccc7b7] px-4 py-2 flex flex-wrap gap-2 justify-center">
        {variables.map((v) => (
          <button
            key={v}
            className={`btn text-sm px-3 py-1 ${
              varStates[v] ? "bg-green-100" : "bg-red-100"
            }`}
            onClick={() => setVarStates((prev) => ({ ...prev, [v]: !prev[v] }))}
          >
            {v}: {varStates[v] ? "ON" : "OFF"}
          </button>
        ))}
      </div>
    </div>
  );
}

export default function Graph({ tree }) {
  return (
    <ReactFlowProvider>
      <GraphInner tree={tree} />
    </ReactFlowProvider>
  );
}
