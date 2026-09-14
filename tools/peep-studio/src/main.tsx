import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "@xyflow/react/dist/style.css";
import App from "./App";
import { EmulatorPopoutApp, isEmulatorPopoutRoute } from "./EmulatorPopoutApp";
import "./styles.css";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    {isEmulatorPopoutRoute() ? <EmulatorPopoutApp /> : <App />}
  </StrictMode>,
);
