import { useEffect } from "react";
import { Routes, Route, useNavigate } from "react-router-dom";
import Home from "./Home.jsx";
import Expr from "./Expr.jsx";
import useWasm from "./hooks/Wasm.js";

export default function App() {
  const wasm = useWasm();
  const navigate = useNavigate();

  useEffect(() => {
    const url = new URL(window.location.href);
    const redirect = url.searchParams.get("redirect");
    if (redirect) {
      navigate(redirect, { replace: true });
    }
  }, []);

  return (
    <Routes>
      <Route path="/" element={<Home wasm={wasm} />} />
      <Route path="/:n" element={<Expr wasm={wasm} />} />
    </Routes>
  );
}
