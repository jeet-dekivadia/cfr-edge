/** @type {import('next').NextConfig} */
const nextConfig = {
  // Allow Three.js and D3 to work server-side safely
  transpilePackages: ['three'],
};

export default nextConfig;
