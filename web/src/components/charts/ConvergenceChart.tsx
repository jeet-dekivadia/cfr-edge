'use client';
import { useEffect, useRef, useMemo } from 'react';
import * as d3 from 'd3';
import type { GameStrategy } from '@/lib/types';
import { ALGO_COLORS } from '@/lib/data';

interface Props {
  strategies: Record<string, GameStrategy>;
  currentIter: number;
  logScale?: boolean;
}

export default function ConvergenceChart({ strategies, currentIter, logScale = true }: Props) {
  const svgRef = useRef<SVGSVGElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);

  const allData = useMemo(() =>
    Object.entries(strategies).map(([algo, s]) => ({
      algo,
      color: ALGO_COLORS[algo] ?? '#94a3b8',
      points: s.convergence,
    })),
    [strategies]
  );

  useEffect(() => {
    if (!svgRef.current || !containerRef.current || allData.length === 0) return;

    const W = containerRef.current.clientWidth || 700;
    const H = 320;
    const M = { top: 20, right: 100, bottom: 45, left: 70 };

    const svg = d3.select(svgRef.current)
      .attr('width', W)
      .attr('height', H);

    svg.selectAll('*').remove();

    const xMax = d3.max(allData, d => d3.max(d.points, p => p.iter)) ?? 10000;
    const yVals = allData.flatMap(d => d.points.map(p => p.exploitability)).filter(v => v > 0);
    const yMin  = d3.min(yVals) ?? 1e-5;
    const yMax  = d3.max(yVals) ?? 1;

    const xScale = d3.scaleLinear().domain([0, xMax]).range([M.left, W - M.right]);
    const yScale = logScale
      ? d3.scaleLog().domain([Math.max(yMin * 0.8, 1e-7), yMax * 1.2]).range([H - M.bottom, M.top]).clamp(true)
      : d3.scaleLinear().domain([0, yMax * 1.1]).range([H - M.bottom, M.top]);

    // Grid
    const xGrid = d3.axisBottom(xScale).ticks(6).tickSize(-(H - M.top - M.bottom)).tickFormat(() => '');
    const yGrid = d3.axisLeft(yScale).ticks(5).tickSize(-(W - M.left - M.right)).tickFormat(() => '');

    svg.append('g').attr('class', 'grid').attr('transform', `translate(0,${H - M.bottom})`)
      .call(xGrid).selectAll('line').style('stroke', '#1e293b').style('stroke-dasharray', '3,3');
    svg.append('g').attr('class', 'grid').attr('transform', `translate(${M.left},0)`)
      .call(yGrid).selectAll('line').style('stroke', '#1e293b').style('stroke-dasharray', '3,3');
    svg.selectAll('.grid .domain').remove();

    // Axes
    svg.append('g').attr('transform', `translate(0,${H - M.bottom})`)
      .call(d3.axisBottom(xScale).ticks(6).tickFormat(d => `${(+d / 1000).toFixed(0)}K`))
      .selectAll('text').style('fill', '#64748b').style('font-size', '11px');
    svg.append('g').attr('transform', `translate(${M.left},0)`)
      .call(logScale
        ? d3.axisLeft(yScale).ticks(5, '.0e')
        : d3.axisLeft(yScale).ticks(5))
      .selectAll('text').style('fill', '#64748b').style('font-size', '11px');

    svg.selectAll('.domain').style('stroke', '#334155');
    svg.selectAll('.tick line').style('stroke', '#334155');

    // Axis labels
    svg.append('text').attr('x', W/2).attr('y', H - 6)
      .attr('text-anchor', 'middle').style('fill', '#64748b').style('font-size', '12px')
      .text('Iterations');
    svg.append('text')
      .attr('transform', `translate(16,${H/2}) rotate(-90)`)
      .attr('text-anchor', 'middle').style('fill', '#64748b').style('font-size', '12px')
      .text('Exploitability (ε)');

    // Theoretical lower bound: ε ≥ C/√T (vanilla CFR)
    const cBound = (yMax * 0.9) * Math.sqrt(100);
    const boundLine = d3.line<number>()
      .x(t => xScale(t))
      .y(t => yScale(Math.min(cBound / Math.sqrt(Math.max(t, 1)), yMax * 10)));
    const boundData = d3.range(1, xMax + 1, xMax / 200);
    svg.append('path').datum(boundData).attr('d', boundLine)
      .style('stroke', '#334155').style('stroke-width', 1.5)
      .style('stroke-dasharray', '6,4').style('fill', 'none').style('opacity', 0.5);
    svg.append('text').attr('x', W - M.right + 5).attr('y', yScale(cBound / Math.sqrt(xMax / 2)) - 5)
      .style('fill', '#475569').style('font-size', '9px').text('O(1/√T)');

    // Lines per algorithm
    const line = d3.line<{ iter: number; exploitability: number }>()
      .x(d => xScale(d.iter))
      .y(d => yScale(Math.max(d.exploitability, 1e-8)))
      .curve(d3.curveMonotoneX);

    for (const { algo, color, points } of allData) {
      const validPoints = points.filter(p => p.exploitability > 0);

      // Area under curve
      const area = d3.area<{ iter: number; exploitability: number }>()
        .x(d => xScale(d.iter))
        .y0(H - M.bottom)
        .y1(d => yScale(Math.max(d.exploitability, 1e-8)))
        .curve(d3.curveMonotoneX);
      svg.append('path').datum(validPoints).attr('d', area)
        .style('fill', color).style('opacity', 0.05);

      // Line
      svg.append('path').datum(validPoints).attr('d', line)
        .style('stroke', color).style('stroke-width', 2.5)
        .style('fill', 'none').style('opacity', 0.9);

      // Legend dot
      const legendY = M.top + Object.keys(ALGO_COLORS).indexOf(algo) * 20;
      svg.append('circle').attr('cx', W - M.right + 12).attr('cy', legendY + 6).attr('r', 5).style('fill', color);
      svg.append('text').attr('x', W - M.right + 22).attr('y', legendY + 10)
        .style('fill', '#94a3b8').style('font-size', '11px').text(algo);
    }

    // Current iteration marker (vertical line)
    if (currentIter > 0) {
      svg.append('line')
        .attr('x1', xScale(currentIter)).attr('x2', xScale(currentIter))
        .attr('y1', M.top).attr('y2', H - M.bottom)
        .style('stroke', '#f59e0b').style('stroke-width', 1.5)
        .style('stroke-dasharray', '4,3').style('opacity', 0.8);

      // Tooltip bubble for current iter
      const firstAlgo = allData[0];
      if (firstAlgo) {
        const closest = firstAlgo.points.reduce((a, b) =>
          Math.abs(a.iter - currentIter) < Math.abs(b.iter - currentIter) ? a : b
        );
        svg.append('circle')
          .attr('cx', xScale(currentIter))
          .attr('cy', yScale(Math.max(closest.exploitability, 1e-8)))
          .attr('r', 5).style('fill', '#f59e0b').style('opacity', 0.9);
      }
    }
  }, [allData, currentIter, logScale]);

  return (
    <div ref={containerRef} className="w-full">
      <svg ref={svgRef} className="overflow-visible" />
    </div>
  );
}
