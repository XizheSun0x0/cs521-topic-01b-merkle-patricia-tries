import type { BranchNode, TrieNode, TrieNodeType } from '@/types/mpt'

export const GRAPH_NODE_WIDTH = 176
export const GRAPH_NODE_HEIGHT = 88
export const GRAPH_LEVEL_GAP = 150
export const GRAPH_SIBLING_GAP = 34
export const GRAPH_TOP_PADDING = 56

export interface GraphNode {
  id: number
  type: TrieNodeType
  x: number
  y: number
  width: number
  height: number
  lines: string[]
}

export interface GraphEdge {
  from: number
  to: number
  fromX: number
  fromY: number
  toX: number
  toY: number
  label: string
}

export interface GraphBounds {
  minX: number
  minY: number
  maxX: number
  maxY: number
  width: number
  height: number
}

export interface TrieGraphLayout {
  nodes: GraphNode[]
  edges: GraphEdge[]
  bounds: GraphBounds
}

interface ChildRelation {
  node: Exclude<TrieNode, null>
  label: string
}

function truncate(value: string, limit = 20): string {
  return value.length <= limit ? value : `${value.slice(0, limit - 1)}…`
}

function countBranchChildren(node: BranchNode): number {
  return node.children.reduce((count, child) => (child?.node ? count + 1 : count), 0)
}

function describeNode(node: Exclude<TrieNode, null>): string[] {
  const lines = [`${node.type} #${node.id}`]

  if (node.type === 'LEAF') {
    lines.push(`path ${node.partial || '<empty>'}`)
    lines.push(truncate(`value ${node.value}`))
    return lines
  }

  if (node.type === 'EXT') {
    lines.push(`path ${node.partial || '<empty>'}`)
    lines.push('redirect')
    return lines
  }

  const occupiedChildren = countBranchChildren(node)
  lines.push(`${occupiedChildren} child${occupiedChildren === 1 ? '' : 'ren'}`)
  if (node.value) {
    lines.push(truncate(`value ${node.value}`))
  }
  else {
    lines.push('no branch value')
  }

  return lines
}

function relationLabel(parent: Exclude<TrieNode, null>, child: Exclude<TrieNode, null>, nibble?: number): string {
  if (parent.type === 'BRANCH') {
    const prefix = nibble?.toString(16) ?? '?'
    if ((child.type === 'EXT' || child.type === 'LEAF') && child.partial) {
      return `${prefix}${child.partial}`
    }
    return prefix
  }

  if ((child.type === 'EXT' || child.type === 'LEAF') && child.partial) {
    return child.partial
  }

  return ''
}

function childRelations(node: Exclude<TrieNode, null>): ChildRelation[] {
  if (node.type === 'LEAF') {
    return []
  }

  if (node.type === 'EXT') {
    return node.child
      ? [{ node: node.child, label: relationLabel(node, node.child) }]
      : []
  }

  return node.children.flatMap((child) => {
    if (!child?.node) {
      return []
    }

    return [{
      node: child.node,
      label: relationLabel(node, child.node, child.nibble),
    }]
  })
}

function buildBounds(nodes: GraphNode[]): GraphBounds {
  if (!nodes.length) {
    return {
      minX: 0,
      minY: 0,
      maxX: GRAPH_NODE_WIDTH,
      maxY: GRAPH_NODE_HEIGHT,
      width: GRAPH_NODE_WIDTH,
      height: GRAPH_NODE_HEIGHT,
    }
  }

  const minX = Math.min(...nodes.map(node => node.x - node.width / 2))
  const maxX = Math.max(...nodes.map(node => node.x + node.width / 2))
  const minY = Math.min(...nodes.map(node => node.y - node.height / 2))
  const maxY = Math.max(...nodes.map(node => node.y + node.height / 2))

  return {
    minX,
    minY,
    maxX,
    maxY,
    width: maxX - minX,
    height: maxY - minY,
  }
}

export function buildTrieGraph(tree: TrieNode): TrieGraphLayout {
  if (!tree) {
    return {
      nodes: [],
      edges: [],
      bounds: buildBounds([]),
    }
  }

  const widthCache = new Map<number, number>()

  const measure = (node: Exclude<TrieNode, null>): number => {
    const cached = widthCache.get(node.id)
    if (cached != null) {
      return cached
    }

    const relations = childRelations(node)
    if (!relations.length) {
      widthCache.set(node.id, GRAPH_NODE_WIDTH)
      return GRAPH_NODE_WIDTH
    }

    const totalChildWidth = relations.reduce((sum, relation, index) => {
      const subtreeWidth = measure(relation.node)
      return sum + subtreeWidth + (index > 0 ? GRAPH_SIBLING_GAP : 0)
    }, 0)

    const width = Math.max(GRAPH_NODE_WIDTH, totalChildWidth)
    widthCache.set(node.id, width)
    return width
  }

  const nodes: GraphNode[] = []
  const edges: GraphEdge[] = []

  const place = (node: Exclude<TrieNode, null>, depth: number, startX: number): { centerX: number, centerY: number } => {
    const nodeWidth = widthCache.get(node.id) ?? GRAPH_NODE_WIDTH
    const centerX = startX + nodeWidth / 2
    const centerY = GRAPH_TOP_PADDING + depth * GRAPH_LEVEL_GAP

    nodes.push({
      id: node.id,
      type: node.type,
      x: centerX,
      y: centerY,
      width: GRAPH_NODE_WIDTH,
      height: GRAPH_NODE_HEIGHT,
      lines: describeNode(node),
    })

    const relations = childRelations(node)
    if (!relations.length) {
      return { centerX, centerY }
    }

    const totalChildWidth = relations.reduce((sum, relation, index) => {
      const subtreeWidth = widthCache.get(relation.node.id) ?? GRAPH_NODE_WIDTH
      return sum + subtreeWidth + (index > 0 ? GRAPH_SIBLING_GAP : 0)
    }, 0)

    let childStartX = startX + (nodeWidth - totalChildWidth) / 2

    for (const relation of relations) {
      const subtreeWidth = widthCache.get(relation.node.id) ?? GRAPH_NODE_WIDTH
      const childPosition = place(relation.node, depth + 1, childStartX)

      edges.push({
        from: node.id,
        to: relation.node.id,
        fromX: centerX,
        fromY: centerY,
        toX: childPosition.centerX,
        toY: childPosition.centerY,
        label: relation.label,
      })

      childStartX += subtreeWidth + GRAPH_SIBLING_GAP
    }

    return { centerX, centerY }
  }

  measure(tree)
  place(tree, 0, 0)

  return {
    nodes,
    edges,
    bounds: buildBounds(nodes),
  }
}
