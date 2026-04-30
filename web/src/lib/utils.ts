import { type ClassValue, clsx } from 'clsx';
import { twMerge } from 'tailwind-merge';

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

export function fmt(n: number, decimals = 4): string {
  if (Math.abs(n) < 1e-9) return '0';
  if (Math.abs(n) < 0.001 || Math.abs(n) >= 10000) {
    return n.toExponential(2);
  }
  return n.toFixed(decimals);
}

export function pct(n: number): string {
  return (n * 100).toFixed(1) + '%';
}

// Interpolate between two hex colors
export function lerpColor(hex1: string, hex2: string, t: number): string {
  const r1 = parseInt(hex1.slice(1,3), 16);
  const g1 = parseInt(hex1.slice(3,5), 16);
  const b1 = parseInt(hex1.slice(5,7), 16);
  const r2 = parseInt(hex2.slice(1,3), 16);
  const g2 = parseInt(hex2.slice(3,5), 16);
  const b2 = parseInt(hex2.slice(5,7), 16);
  const r = Math.round(r1 + (r2-r1)*t);
  const g = Math.round(g1 + (g2-g1)*t);
  const b = Math.round(b1 + (b2-b1)*t);
  return `#${r.toString(16).padStart(2,'0')}${g.toString(16).padStart(2,'0')}${b.toString(16).padStart(2,'0')}`;
}

// Color scale for strategy probabilities: 0→slate, 1→emerald
export function probColor(p: number): string {
  return lerpColor('#334155', '#10b981', p);
}

// Color for regret magnitude: 0→slate, high→amber
export function regretColor(r: number, maxR = 1.0): string {
  const t = Math.min(Math.abs(r) / (maxR + 1e-9), 1.0);
  return lerpColor('#1e293b', '#f59e0b', t);
}
