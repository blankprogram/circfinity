/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  theme: {
    extend: {
      colors: {
        paper: "#fefbf3",
        section: "#fdf9ee",
        ink: "#2b2b2b",
        edge: "#ccc7b7",
        accent: "#d3cab2",
        accentHover: "#c2baa4",
        dot: "#d8d1b8",
        logicTrue: "#d4edda",
        logicFalse: "#f8d7da",
      },
    },
  },
  plugins: [],
};
