import React, { useEffect, useState } from 'react'
import { Routes, Route, useParams, Link, useNavigate } from 'react-router-dom'
import './App.css'

function Home() {
  const navigate = useNavigate()
  const [input, setInput] = useState('')
  const go = () => {
    if (/^\d+$/.test(input)) navigate(`/${input}`)
  }
  return (
    <div className="page">
      <h2>Enter an index</h2>
      <input
        placeholder="42"
        value={input}
        onChange={e => setInput(e.target.value)}
      />
      <button onClick={go}>Go</button>
    </div>
  )
}

function Expr() {
  let { n } = useParams()
  const [mod, setMod] = useState(null)
  const [expr, setExpr] = useState('…loading…')
  const [count, setCount] = useState('…')

  useEffect(() => {
    // load WASM once
    const s = document.createElement('script')
    s.src = '/wasm_main.js'
    s.onload = () =>
      window.createModule().then(m => {
        setMod(m)
        setCount(m.get_expr_count())
      })
    document.body.appendChild(s)
  }, [])

  useEffect(() => {
    if (mod) {
      try {
        setExpr(mod.get_expr(n))
      } catch {
        setExpr('Invalid index')
      }
    }
  }, [mod, n])

  return (
    <div className="page">
      <Link to="/">← Home</Link>
      <h2>Expression #{n}</h2>
      <p>Total expressions: {count}</p>
      <pre>{expr}</pre>
    </div>
  )
}

export default function App() {
  return (
    <Routes>
      <Route path="/" element={<Home />} />
      <Route path="/:n" element={<Expr />} />
    </Routes>
  )
}
