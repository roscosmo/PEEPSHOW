import { useEffect, useState } from "react";

const KEY = "peep-studio.editor-preferences.v1";
const defaults = {
  gridVisible: true,
  majorGridVisible: true,
  gridStrength: 18,
  objectBoxes: true,
  labelMode: "hover" as "hover" | "always" | "off",
  thumbnailPlayback: "always" as "hover" | "always" | "off",
  fontPreviewText: "PEEP STUDIO 0123456789 START SETTINGS CREDITS",
};

function readPreferences(): typeof defaults {
  try {
    const value = JSON.parse(localStorage.getItem(KEY) ?? "null");
    if (!value || typeof value !== "object") return defaults;
    return {
      gridVisible: typeof value.gridVisible === "boolean" ? value.gridVisible : defaults.gridVisible,
      majorGridVisible: typeof value.majorGridVisible === "boolean" ? value.majorGridVisible : defaults.majorGridVisible,
      objectBoxes: typeof value.objectBoxes === "boolean" ? value.objectBoxes : defaults.objectBoxes,
      gridStrength: Number.isInteger(value.gridStrength) && value.gridStrength >= 4 && value.gridStrength <= 30 ? value.gridStrength : defaults.gridStrength,
      labelMode: ["hover", "always", "off"].includes(value.labelMode) ? value.labelMode : defaults.labelMode,
      thumbnailPlayback: value.thumbnailPlayback === "off" ? "off" : "always",
      fontPreviewText: typeof value.fontPreviewText === "string" && value.fontPreviewText.trim().length > 0 && value.fontPreviewText.length <= 120
        ? value.fontPreviewText
        : defaults.fontPreviewText,
    };
  } catch {
    return defaults;
  }
}

export function useEditorPreferences() {
  const [preferences, setPreferences] = useState(readPreferences);
  useEffect(() => {
    try {
      localStorage.setItem(KEY, JSON.stringify(preferences));
    } catch {
      // Keep session preferences usable when local storage is unavailable.
    }
  }, [preferences]);
  const update = <K extends keyof typeof defaults>(key: K, value: typeof defaults[K]) =>
    setPreferences(current => ({ ...current, [key]: value }));
  return { preferences, update };
}
