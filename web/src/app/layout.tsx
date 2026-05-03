import type { Metadata } from 'next';
import { Cormorant_Garamond, Inter, JetBrains_Mono } from 'next/font/google';
import './globals.css';
import Nav from '@/components/Nav';

const inter = Inter({ variable: '--font-geist-sans', subsets: ['latin'] });
const jetbrainsMono = JetBrains_Mono({ variable: '--font-geist-mono', subsets: ['latin'] });
const cormorant = Cormorant_Garamond({
  variable: '--font-display',
  subsets: ['latin'],
  weight: ['400', '500', '600', '700'],
});

export const metadata: Metadata = {
  title: {
    default: 'CFR Edge - Poker GTO Solver',
    template: '%s | CFR Edge',
  },
  description:
    'Interactive visualization of a C++ Counterfactual Regret Minimization engine solving Kuhn, Leduc, and abstracted heads-up Texas Hold\'em.',
  keywords: ['poker', 'CFR', 'CFR+', 'DCFR', 'game theory', 'Nash equilibrium', 'Texas Holdem'],
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" className="dark">
      <body className={`${inter.variable} ${jetbrainsMono.variable} ${cormorant.variable} min-h-screen bg-[#050806] font-sans text-slate-100 antialiased`}>
        <Nav />
        <main className="pt-16">{children}</main>
      </body>
    </html>
  );
}
