import React, { useState, useEffect } from "react";
import { useNavigate } from "react-router-dom";

export default function Home({ wasm }) {
  const [search, setSearch] = useState("");
  const [count, setCount] = useState("…");
  const navigate = useNavigate();

  useEffect(() => {
    if (wasm) setCount(wasm.get_expr_count());
  }, [wasm]);

  const doSearch = () => {
    if (/^\d+$/.test(search)) navigate(`/${search}`);
  };

  return (
    <div className="layout-page items-center justify-center px-4">
      <h1 className="text-4xl mb-10 font-serif">CircFinity</h1>

      <div className="w-full max-w-sm space-y-6">
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

        <div className="space-y-2">
          <div className="text-sm">Total expressions</div>
          <pre className="card card-content break-words whitespace-pre-wrap">
            {count}
          </pre>
        </div>
      </div>
    </div>
  );
}
