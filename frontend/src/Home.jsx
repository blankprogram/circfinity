import React, { useState, useEffect } from "react";
import { useNavigate } from "react-router-dom";

export default function Home({ wasm }) {
  const [input, setInput] = useState("");
  const [count, setCount] = useState("…");
  const navigate = useNavigate();

  useEffect(() => {
    if (wasm) setCount(wasm.get_expr_count());
  }, [wasm]);

  const go = () => {
    if (/^\d+$/.test(input)) navigate(`/${input}`);
  };

  return (
    <div className="min-h-screen flex flex-col items-center justify-center px-4">
      <h1 className="text-4xl mb-10">CircFinity</h1>

      <div className="w-full max-w-sm space-y-6">
        <div className="flex">
          <input
            className="input flex-1"
            placeholder="Enter index…"
            value={input}
            onChange={(e) => setInput(e.target.value)}
            onKeyDown={(e) => e.key === "Enter" && go()}
            disabled={!wasm}
          />
          <button onClick={go} disabled={!wasm} className="btn">
            {wasm ? "Go" : "Loading…"}
          </button>
        </div>

        <div className="space-y-2">
          <div className="text-sm">Total expressions</div>
          <pre className="card p-4">{count}</pre>
        </div>
      </div>
    </div>
  );
}
