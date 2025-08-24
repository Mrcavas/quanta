import tailwindcss from "@tailwindcss/vite"
import { defineConfig } from "vite"
import { viteSingleFile } from "vite-plugin-singlefile"
import solidPlugin from "vite-plugin-solid"

export default defineConfig({
  plugins: [solidPlugin(), tailwindcss(), viteSingleFile()],
  server: {
    port: 3000,
  },
  build: {
    target: "esnext",
  },
})
