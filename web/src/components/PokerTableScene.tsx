'use client';
import { Suspense, useRef, useMemo } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, RoundedBox } from '@react-three/drei';
import * as THREE from 'three';

// ── Seat positions for 6 players around an oval table ──────────────────────
const SEAT_ANGLES = [
  Math.PI * 0.5,    // bottom-center  (hero, P0)
  Math.PI * 0.17,   // bottom-right   (P1)
  Math.PI * -0.17,  // right          (P2)
  Math.PI * -0.5,   // top-center     (P3)
  Math.PI * -0.83,  // left           (P4)
  Math.PI * 0.83,   // bottom-left    (P5)
];
const TABLE_RX = 3.1;   // oval x-radius
const TABLE_RZ = 2.05;  // oval z-radius

function seatPosition(angle: number, rx = TABLE_RX, rz = TABLE_RZ): [number, number, number] {
  return [Math.cos(angle) * rx, 0.15, Math.sin(angle) * rz];
}

// Card suits & ranks cycling per seat for visual variety
const SEAT_CARDS: [string, string][] = [
  ['A', '♠'],   // hero - strong hand
  ['K', '♥'],
  ['7', '♦'],
  ['Q', '♣'],
  ['J', '♠'],
  ['2', '♥'],
];

const COMMUNITY_CARDS = ['K', 'Q', '7', null, null]; // flop dealt, turn/river face-down

// ── Main scene ─────────────────────────────────────────────────────────────
function PokerScene() {
  const sceneRef = useRef<THREE.Group>(null);

  useFrame(({ clock }) => {
    if (sceneRef.current) {
      // Very gentle idle sway so the table feels alive
      sceneRef.current.rotation.y = Math.sin(clock.getElapsedTime() * 0.12) * 0.04;
    }
  });

  return (
    <group ref={sceneRef}>
      <OvalTable />
      <CommunityCards />
      <PotChips />
      {SEAT_ANGLES.map((angle, i) => (
        <PlayerSeat key={i} angle={angle} seatIndex={i} />
      ))}
      <DealerButton />
    </group>
  );
}

// ── Oval table body ─────────────────────────────────────────────────────────
function OvalTable() {
  return (
    <group position={[0, -0.62, 0]}>
      {/* Wood rail */}
      <mesh scale={[TABLE_RX + 0.55, 1, TABLE_RZ + 0.42]} receiveShadow castShadow>
        <cylinderGeometry args={[1, 1, 0.24, 128]} />
        <meshStandardMaterial color="#4a2712" roughness={0.48} metalness={0.05} />
      </mesh>

      {/* Green felt surface */}
      <mesh position={[0, 0.15, 0]} scale={[TABLE_RX, 1, TABLE_RZ]} receiveShadow>
        <cylinderGeometry args={[1, 1, 0.05, 128]} />
        <meshStandardMaterial color="#34a866" roughness={0.74} emissive="#0b2f19" emissiveIntensity={0.16} />
      </mesh>

      {/* Gold trim ring along inner edge */}
      <mesh position={[0, 0.19, 0]} rotation={[Math.PI / 2, 0, 0]} scale={[1, TABLE_RZ / TABLE_RX, 1]}>
        <torusGeometry args={[TABLE_RX, 0.055, 6, 96]} />
        <meshStandardMaterial color="#c9a227" roughness={0.28} metalness={0.75} />
      </mesh>

      {/* Table legs - 4 positioned under the oval */}
      {([[-2.4, -2.4], [2.4, -2.4], [2.4, 2.4], [-2.4, 2.4]] as [number,number][]).map(([x, z], i) => (
        <mesh key={i} position={[x, -1.1, z]}>
          <cylinderGeometry args={[0.1, 0.14, 2.0, 8]} />
          <meshStandardMaterial color="#3b1f08" roughness={0.5} />
        </mesh>
      ))}

      {/* Felt logo / centre disc */}
      <mesh position={[0, 0.205, 0]}>
        <cylinderGeometry args={[0.55, 0.55, 0.005, 48]} />
        <meshStandardMaterial color="#257946" roughness={0.86} emissive="#0d3d22" emissiveIntensity={0.12} />
      </mesh>
    </group>
  );
}

// ── Community cards (flop visible, turn/river face-down) ────────────────────
function CommunityCards() {
  const spacing = 0.52;
  const startX  = -(spacing * 2);

  return (
    <group position={[0, -0.28, 0]}>
      {COMMUNITY_CARDS.map((rank, i) => (
        <FloatingCard
          key={i}
          position={[startX + i * spacing, 0, 0]}
          rotY={0}
          rank={rank}
          faceUp={rank !== null}
          phase={i * 0.4}
        />
      ))}
      {/* "FLOP" label - thin dark plane */}
      <mesh position={[0, -0.025, 0.42]}>
        <planeGeometry args={[1.1, 0.12]} />
        <meshStandardMaterial color="#0a1f12" roughness={1} transparent opacity={0.6} />
      </mesh>
    </group>
  );
}

// ── Pot chip stacks in the centre ───────────────────────────────────────────
function PotChips() {
  return (
    <group position={[0, -0.28, -0.72]}>
      <ChipStack position={[-0.28, 0, 0]} count={7} color="#c62828" />
      <ChipStack position={[  0,  0, 0]} count={5} color="#1565c0" />
      <ChipStack position={[ 0.28, 0, 0]} count={4} color="#212121" />
      <ChipStack position={[-0.14, 0, 0.22]} count={3} color="#388e3c" />
      <ChipStack position={[ 0.14, 0, 0.22]} count={6} color="#f57f17" />
    </group>
  );
}

// ── Individual player seat ───────────────────────────────────────────────────
function PlayerSeat({ angle, seatIndex }: { angle: number; seatIndex: number }) {
  const [cx, , cz] = seatPosition(angle);
  const isHero = seatIndex === 0;
  const cards   = SEAT_CARDS[seatIndex];

  // Seat chip count (hero has most)
  const chipCounts = [12, 8, 5, 9, 6, 11];

  return (
    <group position={[cx, -0.3, cz]}>
      {/* Two hole cards side by side, fanned slightly */}
      <FloatingCard
        position={[-0.16, 0, 0]}
        rotY={angle + Math.PI + 0.12}
        rank={isHero ? cards[0] : null}   // hero sees their cards, others face-down
        faceUp={isHero}
        phase={seatIndex * 1.1}
      />
      <FloatingCard
        position={[0.16, 0, 0]}
        rotY={angle + Math.PI - 0.12}
        rank={isHero ? cards[1] : null}
        faceUp={isHero}
        phase={seatIndex * 1.1 + 0.3}
      />

      {/* Player chip stack */}
      <ChipStack
        position={[0, -0.04, isHero ? -0.55 : 0.42]}
        count={chipCounts[seatIndex]}
        color={isHero ? '#d4af37' : '#607d8b'}
        small
      />

      {/* Seat glow ring - hero gets green, active seats get dim amber */}
      <mesh position={[0, -0.06, 0]}>
        <torusGeometry args={[0.38, 0.025, 6, 32]} />
        <meshStandardMaterial
          color={isHero ? '#22c55e' : '#94a3b8'}
          emissive={isHero ? '#16a34a' : '#334155'}
          emissiveIntensity={isHero ? 1.2 : 0.3}
          roughness={0.3}
          metalness={0.6}
        />
      </mesh>

      {/* Action indicator disc (fold = red, call = blue, raise = green) */}
      {seatIndex === 2 && <ActionBadge color="#ef4444" label="F" />}
      {seatIndex === 4 && <ActionBadge color="#3b82f6" label="C" />}
      {seatIndex === 5 && <ActionBadge color="#22c55e" label="R" />}
    </group>
  );
}

// Small coloured badge to indicate fold / call / raise
function ActionBadge({ color }: { color: string; label: string }) {
  return (
    <group position={[0.48, 0.08, 0]}>
      <mesh>
        <cylinderGeometry args={[0.14, 0.14, 0.04, 16]} />
        <meshStandardMaterial color={color} emissive={color} emissiveIntensity={0.6} roughness={0.3} />
      </mesh>
    </group>
  );
}

// ── Dealer button ────────────────────────────────────────────────────────────
function DealerButton() {
  // Seat 1 is dealer this hand
  const [dx, , dz] = seatPosition(SEAT_ANGLES[1]);
  return (
    <group position={[dx * 0.62, -0.26, dz * 0.62]}>
      <mesh>
        <cylinderGeometry args={[0.1, 0.1, 0.035, 24]} />
        <meshStandardMaterial color="#f5f5f5" roughness={0.1} metalness={0.1} />
      </mesh>
      {/* "D" disc */}
      <mesh position={[0, 0.022, 0]}>
        <cylinderGeometry args={[0.065, 0.065, 0.004, 24]} />
        <meshStandardMaterial color="#1a1a1a" roughness={0.5} />
      </mesh>
    </group>
  );
}

// ── Floating card ─────────────────────────────────────────────────────────────
function FloatingCard({
  position,
  rotY,
  rank,
  faceUp,
  phase = 0,
}: {
  position: [number, number, number];
  rotY: number;
  rank: string | null;
  faceUp: boolean;
  phase?: number;
}) {
  const ref = useRef<THREE.Group>(null);

  useFrame(({ clock }) => {
    if (ref.current) {
      ref.current.position.y = position[1] + Math.sin(clock.getElapsedTime() * 1.1 + phase) * 0.018;
    }
  });

  // Pick suit colour based on rank for visual variety
  const suitColor = useMemo(() => {
    if (!rank) return '#ffffff';
    if (['A', 'K'].includes(rank)) return '#c62828';   // hearts/diamonds = red
    if (['Q', 'J'].includes(rank)) return '#1565c0';   // blue for face cards
    return '#1a1a1a';
  }, [rank]);

  return (
    <group ref={ref} position={position} rotation={[0, rotY, 0]}>
      {/* Card body */}
      <RoundedBox args={[0.38, 0.54, 0.010]} radius={0.025} smoothness={4}>
        <meshStandardMaterial
          color={faceUp ? '#f9fafb' : '#1e3a5f'}
          roughness={0.08}
          metalness={0.0}
        />
      </RoundedBox>

      {faceUp && rank && (
        <>
          {/* Rank pip - coloured disc in top-left */}
          <mesh position={[-0.1, 0.17, 0.007]}>
            <circleGeometry args={[0.055, 24]} />
            <meshStandardMaterial color={suitColor} />
          </mesh>
          {/* Rank pip - bottom-right (mirrored) */}
          <mesh position={[0.1, -0.17, 0.007]}>
            <circleGeometry args={[0.055, 24]} />
            <meshStandardMaterial color={suitColor} />
          </mesh>
          {/* Centre larger disc */}
          <mesh position={[0, 0, 0.007]}>
            <circleGeometry args={[0.08, 24]} />
            <meshStandardMaterial color={suitColor} opacity={0.25} transparent />
          </mesh>
        </>
      )}

      {/* Face-down pattern - diagonal lines */}
      {!faceUp && (
        <>
          <mesh position={[0, 0, 0.007]} rotation={[0, 0, Math.PI * 0.25]}>
            <planeGeometry args={[0.38, 0.008]} />
            <meshStandardMaterial color="#2a4a6b" roughness={1} />
          </mesh>
          <mesh position={[0, 0, 0.007]} rotation={[0, 0, -Math.PI * 0.25]}>
            <planeGeometry args={[0.38, 0.008]} />
            <meshStandardMaterial color="#2a4a6b" roughness={1} />
          </mesh>
          <mesh position={[0, 0, 0.007]}>
            <circleGeometry args={[0.07, 24]} />
            <meshStandardMaterial color="#c9a227" roughness={0.4} metalness={0.5} />
          </mesh>
        </>
      )}
    </group>
  );
}

// ── Chip stack ────────────────────────────────────────────────────────────────
function ChipStack({
  position,
  count,
  color,
  small = false,
}: {
  position: [number, number, number];
  count: number;
  color: string;
  small?: boolean;
}) {
  const r = small ? 0.075 : 0.11;
  const h = small ? 0.028 : 0.038;
  const gap = small ? 0.032 : 0.045;

  return (
    <group position={position}>
      {Array.from({ length: count }).map((_, i) => (
        <group key={i} position={[0, i * gap, 0]}>
          {/* Main disc */}
          <mesh>
            <cylinderGeometry args={[r, r, h, 20]} />
            <meshStandardMaterial color={color} roughness={0.3} metalness={0.3} />
          </mesh>
          {/* White edge stripe */}
          <mesh>
            <torusGeometry args={[r * 0.95, h * 0.18, 4, 20]} />
            <meshStandardMaterial color="#ffffff" roughness={0.5} opacity={0.35} transparent />
          </mesh>
          {/* Top face - slightly lighter */}
          <mesh position={[0, h / 2, 0]}>
            <circleGeometry args={[r * 0.88, 20]} />
            <meshStandardMaterial color={color} roughness={0.2} metalness={0.2} emissive={color} emissiveIntensity={0.08} />
          </mesh>
        </group>
      ))}
    </group>
  );
}

// ── Lighting ─────────────────────────────────────────────────────────────────
function Lighting() {
  return (
    <>
      <ambientLight intensity={0.72} />
      {/* Main overhead lamp - warm */}
      <spotLight
        position={[0, 7, 0]}
        angle={0.55}
        penumbra={0.4}
        intensity={4.8}
        color="#fff8e7"
        castShadow
        shadow-mapSize-width={2048}
        shadow-mapSize-height={2048}
      />
      {/* Fill lights from sides */}
      <pointLight position={[-5, 3,  4]} intensity={0.8} color="#7dd3fc" />
      <pointLight position={[ 5, 3, -4]} intensity={1.15} color="#86efac" />
      <pointLight position={[ 0, 2,  5]} intensity={0.62} color="#fff3e0" />
      {/* Subtle rim from below to lift the felt */}
      <pointLight position={[0, -1.5, 0]} intensity={0.15} color="#1b4d2e" />
    </>
  );
}

// ── Root export ───────────────────────────────────────────────────────────────
export default function PokerTableScene() {
  return (
    <Canvas
      camera={{ position: [0, 6.1, 8.2], fov: 45 }}
      shadows
      style={{ width: '100%', height: '100%' }}
      gl={{ antialias: true, toneMapping: THREE.ACESFilmicToneMapping, toneMappingExposure: 1.55 }}
      dpr={[1, 2]}
    >
      <Suspense fallback={null}>
        <color attach="background" args={['#06130a']} />
        <Lighting />
        <PokerScene />
        <OrbitControls
          enablePan={false}
          enableZoom={false}
          maxPolarAngle={Math.PI / 2.5}
          minPolarAngle={Math.PI / 8}
          autoRotate
          autoRotateSpeed={0.35}
          target={[0, -0.3, 0]}
        />
        <fog attach="fog" args={['#03110a', 18, 42]} />
      </Suspense>
    </Canvas>
  );
}
