'use client';

import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { useState } from 'react';
import { Menu, X } from 'lucide-react';
import { cn } from '@/lib/utils';

const LINKS = [
  { href: '/solve', label: 'Solver' },
  { href: '/play', label: 'Play' },
  { href: '/strategy', label: 'Strategy' },
  { href: '/cpp', label: 'C++' },
  { href: '/holdem', label: "Hold'em" },
];

export default function Nav() {
  const path = usePathname();
  const [open, setOpen] = useState(false);

  return (
    <nav
      className="fixed inset-x-0 top-0 z-50 border-b border-emerald-950/80 bg-[#050806]/92 text-white backdrop-blur-xl"
    >
      <div className="mx-auto flex h-16 max-w-5xl items-center justify-between px-5">
        <Link
          href="/"
          className="font-display text-2xl font-semibold tracking-tight text-emerald-50"
          onClick={() => setOpen(false)}
        >
          CFR-Edge
        </Link>

        <div className="hidden items-center gap-1 md:flex">
          {LINKS.map(link => {
            const active = path === link.href;
            return (
              <Link
                key={link.href}
                href={link.href}
                className={cn(
                  'rounded-full px-4 py-2 text-sm transition',
                  active
                    ? 'bg-emerald-300 text-emerald-950'
                    : 'text-slate-400 hover:bg-emerald-300/[0.08] hover:text-emerald-100',
                )}
              >
                {link.label}
              </Link>
            );
          })}
        </div>

        <button
          type="button"
          onClick={() => setOpen(v => !v)}
          className="flex h-9 w-9 items-center justify-center rounded-full border border-emerald-900 text-slate-300 md:hidden"
          aria-label="Toggle navigation"
        >
          {open ? <X size={17} /> : <Menu size={17} />}
        </button>
      </div>

      <div
        className={cn(
          'border-t px-5 py-3 md:hidden',
          open ? 'block' : 'hidden',
          'border-emerald-950 bg-[#050806]',
        )}
      >
        <div className="mx-auto grid max-w-5xl gap-1">
          {LINKS.map(link => (
            <Link
              key={link.href}
              href={link.href}
              onClick={() => setOpen(false)}
              className="rounded-full px-3 py-2 text-sm text-slate-300 hover:bg-emerald-300/[0.06]"
            >
              {link.label}
            </Link>
          ))}
        </div>
      </div>
    </nav>
  );
}
