import React, { useEffect, useState } from 'react';
import './index.css';

export default function App() {
  const [mod, setMod] = useState(null);
  const [expr, setExpr] = useState('');
  const [input, setInput] = useState('');
  const [count, setCount] = useState('…');

  useEffect(() => {
    const script = document.createElement('script');
    script.src = '/wasm_main.js';
    script.onload = () => {
      window.createModule().then((mod) => {
        setMod(mod);
        setCount(mod.get_expr_count());
      });
    };
    document.body.appendChild(script);
  }, []);

  const handleClick = () => {
    if (!mod || !/^\d+$/.test(input)) {
      setExpr('Invalid input');
      return;
    }
    try {
      setExpr(mod.get_expr(input));
    } catch (e) {
      setExpr('Error: ' + e);
    }
  };

  return (
    <div className="min-h-screen bg-[#121212] text-[#f0f0f0] flex flex-col items-center justify-center font-sans px-4">
      <h1 className="text-2xl mb-4">CircFinity</h1>

      <div className="flex flex-col items-center gap-4 w-full max-w-[700px]">
        <div className="flex">
          <input value={input} onChange={(e) => setInput(e.target.value)} placeholder="Enter index..." className="mr-2" />          
            <button onClick={handleClick}>Get Expression</button>
        </div>

        <div className="text-left w-full text-sm text-gray-400">Total expressions:</div>
        <pre className="bg-[#1a1a1a] text-[#f0f0f0] p-4 border border-gray-600 w-full whitespace-pre-wrap break-words rounded">
          {count}
        </pre>

        <div className="text-left w-full text-sm text-gray-400">Expression:</div>
        <pre className="bg-[#1a1a1a] text-[#f0f0f0] p-4 border border-gray-600 w-full whitespace-pre-wrap break-words rounded">
          {expr}
        </pre>
      </div>
    </div>
  );
}
