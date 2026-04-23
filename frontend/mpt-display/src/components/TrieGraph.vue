<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, reactive, ref, watch } from 'vue'
import { buildTrieGraph, GRAPH_NODE_HEIGHT } from '@/utils/trieGraph'
import type { GraphEdge } from '@/utils/trieGraph'
import type { TrieNode, TrieNodeType } from '@/types/mpt'

const props = defineProps<{
  tree: TrieNode
  highlightedNodeIds: number[]
}>()

const containerRef = ref<HTMLElement | null>(null)
const svgRef = ref<SVGSVGElement | null>(null)

const viewport = reactive({
  width: 960,
  height: 640,
})

const transform = reactive({
  x: 120,
  y: 72,
  scale: 1,
})

const layout = computed(() => buildTrieGraph(props.tree))
const highlightedIds = computed(() => new Set(props.highlightedNodeIds))

let resizeObserver: ResizeObserver | null = null
let activePointerId: number | null = null
const dragState = reactive({
  active: false,
  startX: 0,
  startY: 0,
  originX: 0,
  originY: 0,
})

function nodeFill(type: TrieNodeType): string {
  if (type === 'LEAF') {
    return '#4fb874'
  }
  if (type === 'EXT') {
    return '#f39b42'
  }
  return '#8a4dff'
}

function nodeStroke(type: TrieNodeType): string {
  if (type === 'LEAF') {
    return '#2c7d49'
  }
  if (type === 'EXT') {
    return '#aa5c17'
  }
  return '#5d2fba'
}

function edgePath(edge: GraphEdge): string {
  const startY = edge.fromY + GRAPH_NODE_HEIGHT / 2
  const endY = edge.toY - GRAPH_NODE_HEIGHT / 2
  const controlOffset = Math.max((endY - startY) * 0.48, 44)

  return [
    `M ${edge.fromX} ${startY}`,
    `C ${edge.fromX} ${startY + controlOffset}, ${edge.toX} ${endY - controlOffset}, ${edge.toX} ${endY}`,
  ].join(' ')
}

function edgeLabelX(edge: GraphEdge): number {
  return (edge.fromX + edge.toX) / 2
}

function edgeLabelY(edge: GraphEdge): number {
  return (edge.fromY + edge.toY) / 2 - 14
}

function edgeLabelWidth(label: string): number {
  return Math.max(52, label.length * 10 + 18)
}

function edgeHighlighted(edge: GraphEdge): boolean {
  return highlightedIds.value.has(edge.from) && highlightedIds.value.has(edge.to)
}

function fitToView(): void {
  const bounds = layout.value.bounds
  const safeWidth = Math.max(viewport.width, 320)
  const safeHeight = Math.max(viewport.height, 260)

  if (!layout.value.nodes.length) {
    transform.scale = 1
    transform.x = safeWidth / 2
    transform.y = safeHeight / 2
    return
  }

  const horizontalPadding = 96
  const verticalPadding = 120
  const nextScale = Math.min(
    (safeWidth - horizontalPadding) / Math.max(bounds.width, 1),
    (safeHeight - verticalPadding) / Math.max(bounds.height, 1),
    1.15,
  )

  transform.scale = Number.isFinite(nextScale) && nextScale > 0 ? nextScale : 1
  transform.x = safeWidth / 2 - transform.scale * (bounds.minX + bounds.width / 2)
  transform.y = safeHeight / 2 - transform.scale * (bounds.minY + bounds.height / 2)
}

function updateViewport(): void {
  if (!containerRef.value) {
    return
  }

  const rect = containerRef.value.getBoundingClientRect()
  viewport.width = Math.max(rect.width, 320)
  viewport.height = Math.max(rect.height, 320)
}

function onWheel(event: WheelEvent): void {
  event.preventDefault()

  if (!svgRef.value) {
    return
  }

  const rect = svgRef.value.getBoundingClientRect()
  const mouseX = event.clientX - rect.left
  const mouseY = event.clientY - rect.top
  const zoom = event.deltaY < 0 ? 1.12 : 0.9
  const nextScale = Math.min(Math.max(transform.scale * zoom, 0.32), 2.8)
  const graphX = (mouseX - transform.x) / transform.scale
  const graphY = (mouseY - transform.y) / transform.scale

  transform.x = mouseX - graphX * nextScale
  transform.y = mouseY - graphY * nextScale
  transform.scale = nextScale
}

function onPointerDown(event: PointerEvent): void {
  if (!svgRef.value) {
    return
  }

  activePointerId = event.pointerId
  dragState.active = true
  dragState.startX = event.clientX
  dragState.startY = event.clientY
  dragState.originX = transform.x
  dragState.originY = transform.y
  svgRef.value.setPointerCapture(event.pointerId)
}

function onPointerMove(event: PointerEvent): void {
  if (!dragState.active || activePointerId !== event.pointerId) {
    return
  }

  transform.x = dragState.originX + (event.clientX - dragState.startX)
  transform.y = dragState.originY + (event.clientY - dragState.startY)
}

function stopDragging(event?: PointerEvent): void {
  if (event && svgRef.value && svgRef.value.hasPointerCapture(event.pointerId)) {
    svgRef.value.releasePointerCapture(event.pointerId)
  }

  dragState.active = false
  activePointerId = null
}

watch(
  () => props.tree,
  async () => {
    await nextTick()
    updateViewport()
    fitToView()
  },
  { deep: true, immediate: true },
)

onMounted(() => {
  updateViewport()
  fitToView()

  if (containerRef.value) {
    resizeObserver = new ResizeObserver(() => {
      updateViewport()
      fitToView()
    })
    resizeObserver.observe(containerRef.value)
  }
})

onBeforeUnmount(() => {
  resizeObserver?.disconnect()
})
</script>

<template>
  <div ref="containerRef" class="graph-shell">
    <div class="graph-toolbar">
      <div class="graph-metrics">
        <span>{{ layout.nodes.length }} nodes</span>
        <span>{{ layout.edges.length }} edges</span>
      </div>
      <button class="toolbar-button" type="button" @click="fitToView">
        Fit view
      </button>
    </div>

    <svg
      ref="svgRef"
      class="graph-canvas"
      :class="{ dragging: dragState.active }"
      :viewBox="`0 0 ${viewport.width} ${viewport.height}`"
      @wheel="onWheel"
      @pointerdown="onPointerDown"
      @pointermove="onPointerMove"
      @pointerup="stopDragging"
      @pointerleave="stopDragging"
      @pointercancel="stopDragging"
    >
      <defs>
        <pattern id="graph-grid" width="32" height="32" patternUnits="userSpaceOnUse">
          <path d="M 32 0 L 0 0 0 32" fill="none" stroke="rgba(95, 118, 150, 0.18)" stroke-width="1" />
        </pattern>
        <filter id="proof-glow" x="-80%" y="-80%" width="260%" height="260%">
          <feDropShadow dx="0" dy="0" stdDeviation="7" flood-color="#d74b4b" flood-opacity="0.92" />
        </filter>
      </defs>

      <rect width="100%" height="100%" fill="url(#graph-grid)" rx="18" />

      <g v-if="layout.nodes.length" :transform="`translate(${transform.x} ${transform.y}) scale(${transform.scale})`">
        <g class="edge-layer">
          <g v-for="edge in layout.edges" :key="`${edge.from}-${edge.to}`">
            <path
              class="edge-path"
              :class="{ highlighted: edgeHighlighted(edge) }"
              :d="edgePath(edge)"
            />
            <g v-if="edge.label" :transform="`translate(${edgeLabelX(edge)} ${edgeLabelY(edge)})`">
              <rect
                class="edge-label-chip"
                :x="-edgeLabelWidth(edge.label) / 2"
                y="-12"
                :width="edgeLabelWidth(edge.label)"
                height="24"
                rx="12"
              />
              <text class="edge-label-text" text-anchor="middle" dominant-baseline="middle">
                {{ edge.label }}
              </text>
            </g>
          </g>
        </g>

        <g class="node-layer">
          <g v-for="node in layout.nodes" :key="node.id" :transform="`translate(${node.x} ${node.y})`">
            <rect
              class="node-card"
              :class="{ highlighted: highlightedIds.has(node.id) }"
              :x="-node.width / 2"
              :y="-node.height / 2"
              :width="node.width"
              :height="node.height"
              :fill="nodeFill(node.type)"
              :stroke="nodeStroke(node.type)"
              rx="22"
              :filter="highlightedIds.has(node.id) ? 'url(#proof-glow)' : undefined"
            />
            <text class="node-text" text-anchor="middle">
              <tspan
                v-for="(line, index) in node.lines"
                :key="`${node.id}-${index}`"
                x="0"
                :y="-16 + index * 20"
              >
                {{ line }}
              </tspan>
            </text>
          </g>
        </g>
      </g>

      <g v-else class="empty-state">
        <text x="50%" y="46%" text-anchor="middle" class="empty-title">
          The trie is empty.
        </text>
        <text x="50%" y="53%" text-anchor="middle" class="empty-subtitle">
          Insert a key/value pair or load the demo dataset to render the graph.
        </text>
      </g>
    </svg>
  </div>
</template>

<style scoped>
.graph-shell {
  display: grid;
  grid-template-rows: auto minmax(0, 1fr);
  gap: 0.9rem;
  height: var(--graph-height, clamp(520px, 72vh, 920px));
  max-height: var(--graph-max-height, min(180vh, 1200px));
  min-height: 420px;
}

.graph-toolbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
}

.graph-metrics {
  display: flex;
  flex-wrap: wrap;
  gap: 0.65rem;
  color: rgba(44, 58, 81, 0.74);
  font-size: 0.88rem;
  text-transform: uppercase;
  letter-spacing: 0.09em;
}

.toolbar-button {
  border: 1px solid rgba(18, 37, 63, 0.12);
  background: rgba(255, 255, 255, 0.92);
  color: #163255;
  border-radius: 999px;
  padding: 0.58rem 0.95rem;
  font: inherit;
  font-weight: 700;
  cursor: pointer;
  transition:
    transform 140ms ease,
    border-color 140ms ease,
    background 140ms ease;
}

.toolbar-button:hover {
  transform: translateY(-1px);
  border-color: rgba(22, 50, 85, 0.26);
  background: rgba(250, 252, 255, 0.98);
}

.graph-canvas {
  width: 100%;
  height: 100%;
  min-height: 0;
  border-radius: 28px;
  background:
    radial-gradient(circle at top left, rgba(255, 255, 255, 0.95), rgba(239, 244, 255, 0.9)),
    linear-gradient(180deg, rgba(223, 233, 252, 0.8), rgba(245, 248, 253, 0.95));
  border: 1px solid rgba(16, 42, 67, 0.1);
  box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.7);
  cursor: grab;
  touch-action: none;
}

.graph-canvas.dragging {
  cursor: grabbing;
}

.edge-path {
  fill: none;
  stroke: rgba(32, 59, 93, 0.42);
  stroke-width: 3;
  stroke-linecap: round;
  transition:
    stroke 160ms ease,
    stroke-width 160ms ease;
}

.edge-path.highlighted {
  stroke: rgba(215, 75, 75, 0.96);
  stroke-width: 4.5;
}

.edge-label-chip {
  fill: rgba(255, 255, 255, 0.92);
  stroke: rgba(25, 45, 74, 0.14);
}

.edge-label-text {
  fill: #1f3758;
  font-size: 0.78rem;
  font-weight: 800;
  letter-spacing: 0.04em;
}

.node-card {
  stroke-width: 3.2;
  transition:
    stroke 180ms ease,
    transform 180ms ease;
}

.node-card.highlighted {
  stroke: #d74b4b;
  stroke-width: 4.8;
}

.node-text {
  fill: #ffffff;
  font-size: 0.88rem;
  font-weight: 700;
  pointer-events: none;
}

.empty-state {
  fill: #24415f;
}

.empty-title {
  font-size: 1.3rem;
  font-weight: 800;
}

.empty-subtitle {
  font-size: 0.95rem;
  fill: rgba(36, 65, 95, 0.7);
}

@media (max-width: 900px) {
  .graph-shell {
    min-height: 360px;
  }
}
</style>
