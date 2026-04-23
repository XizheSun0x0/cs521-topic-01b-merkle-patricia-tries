import { describe, expect, it } from 'vitest'
import { buildTrieGraph } from '@/utils/trieGraph'
import type { TrieNode } from '@/types/mpt'

describe('buildTrieGraph', () => {
  it('creates positioned nodes and labeled edges from a trie payload', () => {
    const tree: TrieNode = {
      id: 10,
      type: 'EXT',
      partial: '6',
      child: {
        id: 7,
        type: 'BRANCH',
        value: 'verb',
        children: Array.from({ length: 16 }, (_, nibble) => {
          if (nibble === 1) {
            return {
              nibble,
              node: {
                id: 5,
                type: 'LEAF',
                partial: 'a3f',
                value: 'puppy',
              },
            }
          }

          return null
        }),
      },
    }

    const layout = buildTrieGraph(tree)

    expect(layout.nodes).toHaveLength(3)
    expect(layout.edges).toHaveLength(2)
    expect(layout.edges.some(edge => edge.label === '1a3f')).toBe(true)
    expect(layout.edges.some(edge => edge.label === '')).toBe(true)
    expect(layout.bounds.width).toBeGreaterThan(0)
    expect(layout.nodes.find(node => node.id === 5)?.lines[2]).toContain('puppy')
  })
})
