import React, { useState, useEffect, useRef, useCallback } from "react";

// Helper: return next char in A-Z or a-z range towards target
function nextChar(cur, tgt) {
  if (!tgt) return cur; // safeguard against undefined
  const c = cur.charCodeAt(0);
  const t = tgt.charCodeAt(0);
  if (c >= 65 && c <= 90 && t >= 65 && t <= 90) {
    if (c < t) return String.fromCharCode(c + 1);
    if (c > t) return String.fromCharCode(c - 1);
    return cur;
  }
  if (c >= 97 && c <= 122 && t >= 97 && t <= 122) {
    if (c < t) return String.fromCharCode(c + 1);
    if (c > t) return String.fromCharCode(c - 1);
    return cur;
  }
  return tgt;
}

export default function AnimatedText({
  from,
  to,
  interval = 50,
  className = "",
  as: Component = "span",
  ...props
}) {
  const [display, setDisplay] = useState(from);
  const idxRef = useRef(-1);
  const timerRef = useRef(null);

  const safeLength = from.length === to.length;

  useEffect(() => {
    if (process.env.NODE_ENV !== "production" && !safeLength) {
      console.warn(
        `AnimatedText: 'from' and 'to' must be the same length: "${from}" → "${to}"`,
      );
    }
  }, [from, to, safeLength]);

  const animate = useCallback(
    (startText, endText, locator) => {
      if (!safeLength) return;
      clearInterval(timerRef.current);
      idxRef.current = locator(startText, endText);
      setDisplay(startText);
      timerRef.current = setInterval(() => {
        setDisplay((old) => {
          const pos = idxRef.current;
          if (pos < 0) {
            clearInterval(timerRef.current);
            return endText;
          }
          const cur = old[pos];
          const tgt = endText[pos];
          const next = nextChar(cur, tgt);
          if (next === cur) {
            idxRef.current = locator(old, endText);
            return old;
          }
          return old.slice(0, pos) + next + old.slice(pos + 1);
        });
      }, interval);
    },
    [interval, safeLength],
  );

  const lastMismatch = useCallback((a, b) => {
    for (let i = a.length - 1; i >= 0; --i) if (a[i] !== b[i]) return i;
    return -1;
  }, []);
  const firstMismatch = useCallback((a, b) => {
    for (let i = 0; i < a.length; ++i) if (a[i] !== b[i]) return i;
    return -1;
  }, []);

  const start = () => animate(display, to, lastMismatch);
  const stop = () => animate(display, from, firstMismatch);

  useEffect(() => () => clearInterval(timerRef.current), []);

  return (
    <Component
      className={className}
      onMouseEnter={start}
      onMouseLeave={stop}
      {...props}
    >
      {display}
    </Component>
  );
}
