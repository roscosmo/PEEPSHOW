import { useEffect, useRef } from "react";
import type { CompiledAssetFrame, Framebuffer } from "./types";

export function FramebufferCanvas({ framebuffer }: { framebuffer: Framebuffer | null }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (canvas === null) {
      return;
    }
    const context = canvas.getContext("2d");
    if (context === null) {
      return;
    }

    const width = framebuffer?.width ?? 168;
    const height = framebuffer?.height ?? 144;
    canvas.width = width;
    canvas.height = height;
    context.fillStyle = "#eef1ed";
    context.fillRect(0, 0, width, height);

    if (framebuffer === null) {
      context.fillStyle = "#c7cec9";
      context.fillRect(12, 12, width - 24, 1);
      context.fillRect(12, height - 13, width - 24, 1);
      return;
    }

    const encoded = atob(framebuffer.data_base64);
    const image = context.createImageData(width, height);
    for (let y = 0; y < height; y += 1) {
      for (let x = 0; x < width; x += 1) {
        const source = encoded.charCodeAt(y * framebuffer.row_stride_bytes + (x >> 3));
        const black = (source & (0x80 >> (x & 7))) !== 0;
        const target = (y * width + x) * 4;
        const shade = black ? 17 : 242;
        image.data[target] = shade;
        image.data[target + 1] = black ? 20 : 244;
        image.data[target + 2] = black ? 18 : 240;
        image.data[target + 3] = 255;
      }
    }
    context.putImageData(image, 0, 0);
  }, [framebuffer]);

  return <canvas ref={canvasRef} className="preview-canvas" aria-label="168 by 144 display preview" />;
}

export function FramePreviewCanvas({ frame }: { frame: CompiledAssetFrame }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (canvas === null) {
      return;
    }
    const context = canvas.getContext("2d");
    if (context === null) {
      return;
    }

    canvas.width = frame.width;
    canvas.height = frame.height;
    context.clearRect(0, 0, frame.width, frame.height);

    const pixels = atob(frame.pixels_base64);
    const mask = atob(frame.mask_base64);
    const image = context.createImageData(frame.width, frame.height);
    for (let y = 0; y < frame.height; y += 1) {
      for (let x = 0; x < frame.width; x += 1) {
        const byteIndex = y * frame.row_stride_bytes + (x >> 3);
        const bit = 0x80 >> (x & 7);
        const visible = (mask.charCodeAt(byteIndex) & bit) !== 0;
        const black = (pixels.charCodeAt(byteIndex) & bit) !== 0;
        const target = (y * frame.width + x) * 4;
        image.data[target] = black ? 17 : 242;
        image.data[target + 1] = black ? 20 : 244;
        image.data[target + 2] = black ? 18 : 240;
        image.data[target + 3] = visible ? 255 : 0;
      }
    }
    context.putImageData(image, 0, 0);
  }, [frame]);

  return <canvas ref={canvasRef} className="frame-preview-canvas" aria-hidden="true" />;
}
