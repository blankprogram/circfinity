import React, { useState, useEffect } from "react";
import { useParams, Link, useNavigate } from "react-router-dom";
import Graph from "../src/components/Graph";

export default function Expr({ wasm }) {
  const { n } = useParams();
  const navigate = useNavigate();
  const [search, setSearch] = useState(n);
  const [expr, setExpr] = useState("…");
  const [exprTree, setExprTree] = useState(null);
  const [evaluationResult, setEvaluationResult] = useState(null);
  const [truthTable, setTruthTable] = useState([]);

  useEffect(() => {
    if (!wasm) return;
    try {
      const parsed = JSON.parse(wasm.get_expr_full(n));
      setExpr(parsed.expr);
      setExprTree(parsed.tree);
    } catch {
      setExpr("Invalid index");
      setExprTree(null);
    }
  }, [wasm, n]);

  const doSearch = () => {
    if (/^\d+$/.test(search)) navigate(`/${search}`);
  };

  return (
    <div className="layout-page">
      <header className="w-full py-2 px-4 bg-section border-y border-edge flex items-center justify-between">
        <Link to="/" className="block text-2xl font-serif no-underline">
          CircFinity
        </Link>
        <div className="flex items-center">
          <input
            className="input flex-1"
            placeholder="Go to index…"
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            onKeyDown={(e) => e.key === "Enter" && doSearch()}
            disabled={!wasm}
          />
          <button
            className="btn btn-hover btn-large"
            onClick={doSearch}
            disabled={!wasm}
          >
            {wasm ? "Go" : "…"}
          </button>
        </div>
      </header>

      <main className="layout-main grid-cols-1 lg:grid-cols-3">
        <div className="card card-content">
          <h2 className="text-xl font-serif font-normal mb-2 break-words whitespace-pre-wrap">
            Expression #{n}
          </h2>
          <pre className="font-mono break-words whitespace-pre-wrap">
            {expr}
          </pre>
        </div>

        <div className="card overflow-hidden flex">
          <Graph
            tree={exprTree}
            wasm={wasm}
            n={n}
            onEvaluate={setEvaluationResult}
            onTruthTable={setTruthTable}
          />
        </div>

        <div className="card card-content flex flex-col overflow-y-auto">
          <h2 className="text-xl font-serif font-normal mb-2">Output</h2>
          <p className="text-lg font-serif mb-4">
            {evaluationResult != null
              ? `Result: ${evaluationResult === "true" ? "1" : "0"}`
              : "..."}
          </p>

          {truthTable.length > 0 && truthTable[0]?.inputs && (
            <>
              <h3 className="text-base font-serif font-normal mb-1">
                Current Evaluation
              </h3>
              <div className="border-t border-edge mb-2" />
              <div className="overflow-auto grow">
                <table className="table-custom table-sep font-serif text-md">
                  <thead>
                    <tr>
                      <th>Variable</th>
                      <th>Value</th>
                    </tr>
                  </thead>
                  <tbody>
                    {Object.entries(truthTable[0].inputs).map(([v, val]) => (
                      <tr key={v}>
                        <td>{v}</td>
                        <td>{val ? 1 : 0}</td>
                      </tr>
                    ))}
                    <tr>
                      <td className="font-normal">Output</td>
                      <td className="font-normal">
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
