import React from "react";
import { createRoot } from "react-dom/client";
import { HashRouter, Routes, Route } from "react-router-dom";
import Home from "./Home.jsx";
import Expr from "./Expr.jsx";
import useWasm from "./hooks/Wasm.js";
import "./index.css";

function App() {
  const wasm = useWasm();

  return (
    <HashRouter>
      <Routes>
        <Route path="/" element={<Home wasm={wasm} />} />
        <Route path="/:n" element={<Expr wasm={wasm} />} />
      </Routes>
    </HashRouter>
  );
}

createRoot(document.getElementById("root")).render(<App />);
