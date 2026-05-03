# CFR-Edge Web

Next.js 16 / React 19 visualization app for the CFR-Edge C++ poker solver.

The app is static-data driven: all solver output is read from `public/strategies/`, so Vercel does not need to run the C++ binaries or host an API.

## Local Development

```bash
npm install
npm run dev
```

Open `http://localhost:3000`.

## Verification

```bash
npm run lint
npm run build
npm audit --omit=dev
```

## Vercel Notes

- Framework preset: Next.js
- Root directory: `web`
- Node.js: 20.9 or newer
- Build command: `npm run build`
- Output: Next.js default
- Required runtime services: none

Regenerate strategy bundles from the repository root with:

```bash
./build/json_exporter --out ./web/public/strategies/
```
