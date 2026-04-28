import type { Metadata } from 'next';
import { Geist, Geist_Mono } from 'next/font/google';
import './globals.css';
import Nav from '@/components/Nav';

const geistSans = Geist({ variable: '--font-geist-sans', subsets: ['latin'] });
const geistMono = Geist_Mono({ variable: '--font-geist-mono', subsets: ['latin'] });

export const metadata: Metadata = {
  title: 'CFR Edge — Poker GTO Solver',
  description:
    'Real-time visualisation of Counterfactual Regret Minimization solving poker to Nash equilibrium. CFR / CFR+ / DCFR on Kuhn, Leduc, and Texas Hold\'em.',
  keywords: ['poker', 'CFR', 'game theory', 'Nash equilibrium', 'quantitative finance'],
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" className="dark">
      <body className={`${geistSans.variable} ${geistMono.variable} antialiased bg-gray-950 text-gray-100 min-h-screen`}>
        <Nav />
        <main className="pt-16">{children}</main>
      </body>
    </html>
  );
}
