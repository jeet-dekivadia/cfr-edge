'use client';
import { Suspense, useRef } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, RoundedBox, Text } from '@react-three/drei';
import * as THREE from 'three';

function PokerTable() {
  const tableRef = useRef<THREE.Mesh>(null);
  useFrame(({ clock }) => {
    if (tableRef.current) {
      tableRef.current.rotation.y = Math.sin(clock.getElapsedTime() * 0.15) * 0.08;
    }
  });

  return (
    <group ref={tableRef} position={[0, -0.5, 0]}>
      {/* Rail (outer wooden edge) */}
      <mesh position={[0, 0, 0]}>
        <cylinderGeometry args={[3.6, 3.6, 0.18, 64]} />
        <meshStandardMaterial color="#4a2c0a" roughness={0.4} metalness={0.05} />
      </mesh>

      {/* Felt surface */}
      <mesh position={[0, 0.1, 0]}>
        <cylinderGeometry args={[3.2, 3.2, 0.06, 64]} />
        <meshStandardMaterial color="#1a472a" roughness={0.9} />
      </mesh>

      {/* Inner rail ring highlight */}
      <mesh position={[0, 0.08, 0]}>
        <torusGeometry args={[3.2, 0.06, 8, 64]} />
        <meshStandardMaterial color="#d4af37" roughness={0.3} metalness={0.7} />
      </mesh>

      {/* Cards - Player 1 */}
      <FloatingCard position={[-0.3, 0.14, 1.6]} rotation={[0, 0, -0.15]} label="?" />
      <FloatingCard position={[ 0.3, 0.14, 1.6]} rotation={[0, 0,  0.15]} label="?" />

      {/* Cards - Player 2 */}
      <FloatingCard position={[-0.3, 0.14, -1.6]} rotation={[Math.PI, 0, -0.15]} label="K" faceUp />
      <FloatingCard position={[ 0.3, 0.14, -1.6]} rotation={[Math.PI, 0,  0.15]} label="♠" faceUp />

      {/* Chips in pot */}
      <ChipStack position={[0, 0.13, 0]} count={5} color="#c62828" />
      <ChipStack position={[0.25, 0.13, 0.1]} count={3} color="#1565c0" />

      {/* Table legs */}
      {[0, Math.PI/2, Math.PI, Math.PI*1.5].map((angle, i) => (
        <mesh
          key={i}
          position={[Math.cos(angle) * 2.8, -1.2, Math.sin(angle) * 2.8]}
        >
          <cylinderGeometry args={[0.12, 0.18, 2.2, 8]} />
          <meshStandardMaterial color="#3b1f08" roughness={0.5} />
        </mesh>
      ))}
    </group>
  );
}

function FloatingCard({
  position,
  rotation,
  label,
  faceUp = false,
}: {
  position: [number, number, number];
  rotation: [number, number, number];
  label: string;
  faceUp?: boolean;
}) {
  const ref = useRef<THREE.Group>(null);
  useFrame(({ clock }) => {
    if (ref.current) {
      ref.current.position.y = position[1] + Math.sin(clock.getElapsedTime() * 1.2 + position[0]) * 0.025;
    }
  });

  return (
    <group ref={ref} position={position} rotation={rotation}>
      <RoundedBox args={[0.42, 0.6, 0.012]} radius={0.03}>
        <meshStandardMaterial
          color={faceUp ? '#f8f9fa' : '#1e3a5f'}
          roughness={0.1}
          metalness={0.0}
        />
      </RoundedBox>
      {faceUp && (
        <Text
          position={[0, 0, 0.008]}
          fontSize={0.2}
          color="#111"
          anchorX="center"
          anchorY="middle"
          font="/fonts/geist-bold.woff"
        >
          {label}
        </Text>
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
          <cylinderGeometry args={[0.11, 0.11, 0.038, 16]} />
          <meshStandardMaterial color={color} roughness={0.3} metalness={0.3} />
        </mesh>
      ))}
    </group>
  );
}

function Lighting() {
  return (
    <>
      <ambientLight intensity={0.3} />
      <pointLight position={[0, 5, 0]} intensity={1.2} color="#fffaf0" castShadow />
      <pointLight position={[-4, 3, 4]}  intensity={0.4} color="#4fc3f7" />
      <pointLight position={[ 4, 3, -4]} intensity={0.4} color="#81c784" />
      <spotLight
        position={[0, 8, 0]}
        angle={0.5}
        penumbra={0.3}
        intensity={2}
        castShadow
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
          autoRotateSpeed={0.4}
        />
        <fog attach="fog" args={['#030712', 12, 30]} />
      </Suspense>
    </Canvas>
  );
}
