'use client';
import { Suspense, useRef } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, RoundedBox } from '@react-three/drei';
import * as THREE from 'three';

function PokerTable() {
  const tableRef = useRef<THREE.Group>(null);
  useFrame(({ clock }) => {
    if (tableRef.current) {
      tableRef.current.rotation.y = Math.sin(clock.getElapsedTime() * 0.15) * 0.08;
    }
  });

  return (
    <group ref={tableRef} position={[0, -0.5, 0]}>
      {/* Rail — outer wooden edge */}
      <mesh position={[0, 0, 0]}>
        <cylinderGeometry args={[3.6, 3.6, 0.18, 64]} />
        <meshStandardMaterial color="#4a2c0a" roughness={0.4} metalness={0.05} />
      </mesh>

      {/* Felt surface */}
      <mesh position={[0, 0.1, 0]}>
        <cylinderGeometry args={[3.2, 3.2, 0.06, 64]} />
        <meshStandardMaterial color="#1a472a" roughness={0.9} />
      </mesh>

      {/* Gold inner rail ring */}
      <mesh position={[0, 0.08, 0]}>
        <torusGeometry args={[3.2, 0.06, 8, 64]} />
        <meshStandardMaterial color="#d4af37" roughness={0.3} metalness={0.7} />
      </mesh>

      {/* Cards — Player 1 (face down) */}
      <FloatingCard position={[-0.3, 0.14,  1.6]} rotZ={-0.15} faceUp={false} />
      <FloatingCard position={[ 0.3, 0.14,  1.6]} rotZ={ 0.15} faceUp={false} />

      {/* Cards — Player 2 (face up, white) */}
      <FloatingCard position={[-0.3, 0.14, -1.6]} rotZ={-0.15} faceUp />
      <FloatingCard position={[ 0.3, 0.14, -1.6]} rotZ={ 0.15} faceUp />

      {/* Chip stacks in the pot */}
      <ChipStack position={[  0,    0.13,  0  ]} count={5} color="#c62828" />
      <ChipStack position={[ 0.25,  0.13,  0.1]} count={3} color="#1565c0" />
      <ChipStack position={[-0.22,  0.13, -0.1]} count={4} color="#212121" />

      {/* Table legs */}
      {([0, Math.PI / 2, Math.PI, Math.PI * 1.5] as number[]).map((angle, i) => (
        <mesh key={i} position={[Math.cos(angle) * 2.8, -1.2, Math.sin(angle) * 2.8]}>
          <cylinderGeometry args={[0.12, 0.18, 2.2, 8]} />
          <meshStandardMaterial color="#3b1f08" roughness={0.5} />
        </mesh>
      ))}
    </group>
  );
}

function FloatingCard({
  position,
  rotZ,
  faceUp,
}: {
  position: [number, number, number];
  rotZ: number;
  faceUp: boolean;
}) {
  const ref = useRef<THREE.Group>(null);
  // Gentle floating animation, each card at slightly different phase
  useFrame(({ clock }) => {
    if (ref.current) {
      ref.current.position.y =
        position[1] + Math.sin(clock.getElapsedTime() * 1.2 + position[0] * 3) * 0.025;
    }
  });

  return (
    <group ref={ref} position={position} rotation={[0, 0, rotZ]}>
      {/* Card body */}
      <RoundedBox args={[0.42, 0.6, 0.012]} radius={0.03} smoothness={4}>
        <meshStandardMaterial
          color={faceUp ? '#f8f9fa' : '#1e3a5f'}
          roughness={0.12}
          metalness={0.0}
        />
      </RoundedBox>

      {/* Suit / rank marker as a small coloured disc — avoids needing font files */}
      {faceUp && (
        <mesh position={[0, 0, 0.009]}>
          <circleGeometry args={[0.09, 32]} />
          <meshStandardMaterial color="#c62828" />
        </mesh>
      )}

      {/* Back pattern for face-down cards */}
      {!faceUp && (
        <mesh position={[0, 0, 0.009]}>
          <planeGeometry args={[0.34, 0.52]} />
          <meshStandardMaterial color="#152b4a" roughness={0.8} />
        </mesh>
      )}
    </group>
  );
}

function ChipStack({
  position,
  count,
  color,
}: {
  position: [number, number, number];
  count: number;
  color: string;
}) {
  return (
    <group position={position}>
      {Array.from({ length: count }).map((_, i) => (
        <mesh key={i} position={[0, i * 0.045, 0]}>
          <cylinderGeometry args={[0.11, 0.11, 0.038, 24]} />
          <meshStandardMaterial color={color} roughness={0.3} metalness={0.35} />
        </mesh>
      ))}
      {/* Edge stripe on each chip */}
      {Array.from({ length: count }).map((_, i) => (
        <mesh key={`stripe-${i}`} position={[0, i * 0.045, 0]}>
          <torusGeometry args={[0.108, 0.008, 4, 24]} />
          <meshStandardMaterial color="#ffffff" roughness={0.5} opacity={0.4} transparent />
        </mesh>
      ))}
    </group>
  );
}

function Lighting() {
  return (
    <>
      <ambientLight intensity={0.35} />
      <pointLight position={[0, 5, 0]}   intensity={1.2} color="#fffaf0" />
      <pointLight position={[-4, 3, 4]}  intensity={0.45} color="#4fc3f7" />
      <pointLight position={[ 4, 3, -4]} intensity={0.45} color="#81c784" />
      <spotLight
        position={[0, 8, 0]}
        angle={0.5}
        penumbra={0.3}
        intensity={1.8}
        castShadow
        shadow-mapSize-width={1024}
        shadow-mapSize-height={1024}
      />
    </>
  );
}

export default function PokerTableScene() {
  return (
    <Canvas
      camera={{ position: [0, 5, 8], fov: 45 }}
      shadows
      style={{ width: '100%', height: '100%' }}
      gl={{ antialias: true }}
      dpr={[1, 2]}
    >
      <Suspense fallback={null}>
        <Lighting />
        <PokerTable />
        <OrbitControls
          enablePan={false}
          enableZoom={false}
          maxPolarAngle={Math.PI / 2.2}
          minPolarAngle={Math.PI / 6}
          autoRotate
          autoRotateSpeed={0.5}
        />
        <fog attach="fog" args={['#030712', 14, 32]} />
      </Suspense>
    </Canvas>
  );
}
