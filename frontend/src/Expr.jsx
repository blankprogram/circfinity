import React, { useState, useEffect } from "react";
import { useParams, Link, useNavigate } from "react-router-dom";
import Graph from "../src/components/Graph.jsx";

export default function Expr({ wasm }) {
  const { n } = useParams();
  const navigate = useNavigate();
  const [search, setSearch] = useState(n);
  const [expr, setExpr] = useState("…loading…");
  const [exprTree, setExprTree] = useState(null);
  const [evaluationResult, setEvaluationResult] = useState(null);
  const [truthTable, setTruthTable] = useState([]);

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
    <div className="h-screen flex flex-col bg-[#fefbf3] text-[#2b2b2b]">
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

      <main className="flex-1 grid grid-cols-1 lg:grid-cols-3 gap-4 p-6 overflow-hidden">
        <div className="card p-4 h-full overflow-auto">
          <h2 className="text-xl font-semibold mb-2">Expression #{n}</h2>
          <pre className="font-mono whitespace-pre-wrap break-words w-full">
            {expr}
          </pre>
        </div>

        <div className="border border-[#ccc7b7] bg-[#fdf9ee] min-h-[600px] h-full overflow-hidden flex">
          <Graph
            tree={exprTree}
            wasm={wasm}
            n={n}
            onEvaluate={setEvaluationResult}
            onTruthTable={setTruthTable}
          />
        </div>

        <div className="card p-4 h-full flex flex-col overflow-y-auto">
          <h2 className="text-xl font-semibold mb-2">Output</h2>
          <p className="text-lg font-mono mb-4">
            {evaluationResult != null
              ? `Result: ${evaluationResult === "true" ? "1" : "0"}`
              : "Toggle inputs to see result"}
          </p>

          {truthTable.length > 0 && truthTable[0]?.inputs && (
            <>
              <h3 className="font-semibold mb-1">Current Evaluation</h3>
              <div className="overflow-auto grow">
                <table className="font-mono text-sm border border-gray-300 w-full">
                  <thead>
                    <tr>
                      <th className="px-2 py-1 border-b text-left">Variable</th>
                      <th className="px-2 py-1 border-b text-left">Value</th>
                    </tr>
                  </thead>
                  <tbody>
                    {Object.entries(truthTable[0].inputs).map(([v, val]) => (
                      <tr key={v}>
                        <td className="px-2 py-1 border-b">{v}</td>
                        <td className="px-2 py-1 border-b">{val ? 1 : 0}</td>
                      </tr>
                    ))}
                    <tr>
                      <td className="px-2 py-1 font-semibold">Output</td>
                      <td className="px-2 py-1 font-semibold">
                        {truthTable[0].output === "true" ? 1 : 0}
                      </td>
                    </tr>
                  </tbody>
                </table>
              </div>
            </>
          )}
        </div>
      </main>
    </div>
  );
}
