import React, { useState, useEffect } from "react";
import { useParams, Link, useNavigate } from "react-router-dom";

export default function Expr({ wasm }) {
  const { n } = useParams();
  const navigate = useNavigate();

  const [search, setSearch] = useState(n);
  const [expr, setExpr] = useState("…loading…");

  useEffect(() => {
    if (!wasm) return;
    try {
      setExpr(wasm.get_expr(n));
    } catch {
      setExpr("Invalid index");
    }
  }, [wasm, n]);

  const doSearch = () => {
    if (/^\d+$/.test(search)) navigate(`/${search}`);
  };

  return (
    <div className="min-h-screen flex flex-col">
      <header className="border-y w-full py-2 px-4 flex items-center justify-between">
        <Link to="/" className="text-xl">
          CircFinity
        </Link>
        <div className="flex">
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

      <main className="flex-1 p-6 flex flex-col items-center">
        <div className="w-full max-w-md space-y-6 mt-8">
          <h1 className="text-2xl text-center">Expression</h1>

          <div className="card px-4 py-1 text-center">#{n}</div>

          <pre className="card p-4">{expr}</pre>
        </div>
      </main>
    </div>
  );
}
