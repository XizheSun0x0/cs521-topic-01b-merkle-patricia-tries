export type TrieNodeType = 'LEAF' | 'EXT' | 'BRANCH'

export interface EntryRecord {
  key: string
  value: string
}

export interface RootResponse {
  root: string
}

export interface HealthResponse {
  status: string
}

export interface EntriesResponse {
  entries: EntryRecord[]
}

export interface MutationResponse {
  ok?: boolean
  deleted?: boolean
  root: string
  count?: number
  error?: string
}

export interface GetResponse {
  found: boolean
  value?: string
  proof_ids?: number[]
}

export interface ProofNodeInfo {
  index: number
  hash: string
  size: number
}

export interface ProofResponse {
  found: boolean
  value?: string
  verified?: boolean
  proof_length?: number
  proof_ids?: number[]
  nodes?: ProofNodeInfo[]
}

export interface VerifyResponse {
  verified: boolean
}

export interface LeafNode {
  id: number
  type: 'LEAF'
  partial: string
  value: string
}

export interface ExtensionNode {
  id: number
  type: 'EXT'
  partial: string
  child: TrieNode
}

export interface BranchChild {
  nibble: number
  node: TrieNode
}

export interface BranchNode {
  id: number
  type: 'BRANCH'
  children: Array<BranchChild | null>
  value?: string
}

export type TrieNode = LeafNode | ExtensionNode | BranchNode | null

export interface TreeResponse {
  tree: TrieNode
}

export type ConnectionStatus = 'checking' | 'connected' | 'offline'

export type MessageTone = 'success' | 'error' | 'info'

export interface StatusMessage {
  id: number
  tone: MessageTone
  title: string
  detail: string
}

export interface ProofPanelState {
  source: 'lookup' | 'proof'
  key: string
  found: boolean
  value: string
  proofIds: number[]
  proofLength: number
  verified: boolean | null
  nodes: ProofNodeInfo[]
}
