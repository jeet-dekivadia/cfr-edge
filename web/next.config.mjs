/** @type {import('next').NextConfig} */
const nextConfig = {
  // Allow Three.js and D3 to work server-side safely
  transpilePackages: ['three'],
  devIndicators: false,

  // Disable source maps in production to avoid leaking source code
  productionBrowserSourceMaps: false,

  // Security headers
  async headers() {
    return [
      {
        source: '/(.*)',
        headers: [
          { key: 'X-Content-Type-Options', value: 'nosniff' },
          { key: 'X-Frame-Options', value: 'DENY' },
          { key: 'Referrer-Policy', value: 'strict-origin-when-cross-origin' },
          { key: 'Permissions-Policy', value: 'camera=(), microphone=(), geolocation=()' },
        ],
      },
    ];
  },
};

export default nextConfig;
