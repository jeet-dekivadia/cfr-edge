'use client';
import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { cn } from '@/lib/utils';

const LINKS = [
  { href: '/',          label: 'Home'      },
  { href: '/solve',     label: 'Solver'    },
  { href: '/play',      label: 'Play'      },
  { href: '/strategy',  label: 'Strategy'  },
  { href: '/algorithm', label: 'Algorithm' },
  { href: '/holdem',    label: "Hold'em"   },
];

export default function Nav() {
  const path = usePathname();
  return (
    <nav className="fixed top-0 left-0 right-0 z-50 h-16
                    bg-gray-950/90 backdrop-blur-md
                    border-b border-gray-800/60">
      <div className="max-w-7xl mx-auto px-4 h-full flex items-center justify-between">
        {/* Logo */}
        <Link href="/" className="flex items-center gap-2 group">
          <span className="text-2xl font-bold tracking-tight text-white group-hover:text-emerald-400 transition-colors">
            CFR<span className="text-emerald-400">Edge</span>
          </span>
          <span className="text-xs text-gray-500 hidden sm:block font-mono">
            v2 • Nash Equilibrium Solver
          </span>
        </Link>

        {/* Nav links */}
        <div className="flex items-center gap-1">
          {LINKS.map(({ href, label }) => (
            <Link
              key={href}
              href={href}
              className={cn(
                'px-3 py-1.5 rounded-md text-sm font-medium transition-all',
                path === href
                  ? 'bg-emerald-500/20 text-emerald-400 ring-1 ring-emerald-500/30'
                  : 'text-gray-400 hover:text-white hover:bg-gray-800'
              )}
            >
              {label}
            </Link>
          ))}
        </div>
      </div>
    </nav>
  );
}
