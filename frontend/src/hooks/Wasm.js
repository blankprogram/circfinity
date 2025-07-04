import { useState, useEffect } from 'react';

let modulePromise = null;

export default function useWasm() {
  const [mod, setMod] = useState(null);

  useEffect(() => {
    if (!modulePromise) {
      const s = document.createElement('script');
      s.src = '/wasm_main.js';
      document.body.appendChild(s);

      modulePromise = new Promise(resolve => {
        s.onload = () => window.createModule().then(resolve);
      });
    }
    modulePromise.then(setMod);
  }, []);

  return mod;
}
