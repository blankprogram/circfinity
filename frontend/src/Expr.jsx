import React, { useState, useEffect } from "react";
import { useParams, Link, useNavigate } from "react-router-dom";
import { ReactFlowProvider } from "reactflow";
import Graph from "../src/components/Graph.jsx";

export default function Expr({ wasm }) {
  const { n } = useParams();
  const navigate = useNavigate();
  const [search, setSearch] = useState(n);
  const [expr, setExpr] = useState("…loading…");
  const [exprTree, setExprTree] = useState(null);

  useEffect(() => {
    if (!wasm) return;
    try {
      setExpr(wasm.get_expr(n));
      setExprTree(JSON.parse(wasm.get_expr_full(n)).tree);
    } catch (err) {
      console.error(err);
      setExpr("Invalid index");
      setExprTree(null);
    }
  }, [wasm, n]);

  const doSearch = () => /^\d+$/.test(search) && navigate(`/${search}`);

  return (
    <div className="min-h-screen flex flex-col bg-[#fefbf3] text-[#2b2b2b]">
      <header className="border-y w-full py-2 px-4 flex items-center justify-between">
        <Link to="/" className="text-xl font-serif">
          CircFinity
        </Link>
        <div className="flex gap-2">
          <input
            className="input"
            placeholder="Go to index…"
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            onKeyDown={(e) => e.key === "Enter" && doSearch()}
          />
          <button onClick={doSearch} className="btn">
            Go
          </button>
        </div>
      </header>

      <main className="flex-1 grid grid-cols-1 lg:grid-cols-3 gap-4 p-6">
        <div className="card p-4 h-full overflow-auto">
          <h2 className="text-xl font-semibold mb-2">Expression #{n}</h2>
          <pre className="font-mono whitespace-pre-wrap break-words w-full">
            {expr}
          </pre>
        </div>

        <div className="border border-[#ccc7b7] bg-[#fdf9ee] min-h-[600px] h-full overflow-hidden flex">
          <ReactFlowProvider>
            <Graph tree={exprTree} />
          </ReactFlowProvider>
        </div>

        <div className="card p-4 h-full overflow-auto">
          <h2 className="text-xl font-semibold mb-2">Coming Soon</h2>
          <p className="text-gray-600">
            This panel can show truth tables, subtree editing, etc.
          </p>
        </div>
      </main>
    </div>
  );
}
