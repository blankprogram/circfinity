// src/main.jsx
import React from 'react'
import { createRoot } from 'react-dom/client'
import { BrowserRouter, Routes, Route } from 'react-router-dom'
import Home from './Home.jsx'
import Expr from './Expr.jsx'
import useWasm from './hooks/Wasm.js'
import './index.css'

function App() {
  // load wasm only once
  const wasm = useWasm()

  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Home wasm={wasm} />} />
        <Route path="/:n" element={<Expr wasm={wasm} />} />
      </Routes>
    </BrowserRouter>
  )
}

createRoot(document.getElementById('root')).render(<App />)
