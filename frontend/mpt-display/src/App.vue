<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import TrieGraph from '@/components/TrieGraph.vue'
import type {
  ConnectionStatus,
  EntriesResponse,
  EntryRecord,
  GetResponse,
  HealthResponse,
  MessageTone,
  MutationResponse,
  ProofPanelState,
  ProofResponse,
  RootResponse,
  StatusMessage,
  TreeResponse,
  TrieNode,
} from '@/types/mpt'

const DEFAULT_BASE_URL = 'http://localhost:8080'
const MESSAGE_LIMIT = 6

const baseUrl = ref(loadStoredBaseUrl())
const connectionStatus = ref<ConnectionStatus>('checking')
const rootHash = ref('')
const entries = ref<EntryRecord[]>([])
const tree = ref<TrieNode>(null)
const keyInput = ref('')
const valueInput = ref('')
const proofState = ref<ProofPanelState | null>(null)
const proofLoading = ref(false)
const statusMessages = ref<StatusMessage[]>([])
const refreshing = ref(false)
const mutating = ref(false)
const querying = ref(false)

const highlightedNodeIds = computed(() => proofState.value?.proofIds ?? [])
const rootPreview = computed(() => {
  if (!rootHash.value) {
    return 'Unavailable'
  }

  return rootHash.value
})

const connectionLabel = computed(() => {
  if (connectionStatus.value === 'connected') {
    return 'Connected'
  }

  if (connectionStatus.value === 'checking') {
    return 'Checking'
  }

  return 'Offline'
})

function loadStoredBaseUrl(): string {
  if (typeof window === 'undefined') {
    return DEFAULT_BASE_URL
  }

  return normalizeBaseUrl(window.localStorage.getItem('mpt-display-base-url') || DEFAULT_BASE_URL)
}

function normalizeBaseUrl(value: string): string {
  const trimmed = value.trim()
  if (!trimmed) {
    return DEFAULT_BASE_URL
  }

  return trimmed.replace(/\/+$/, '')
}

function statusToneClass(tone: MessageTone): string {
  return `message-${tone}`
}

function connectionToneClass(status: ConnectionStatus): string {
  return `status-${status}`
}

function pushStatus(tone: MessageTone, title: string, detail: string): void {
  statusMessages.value = [
    {
      id: Date.now() + Math.floor(Math.random() * 1000),
      tone,
      title,
      detail,
    },
    ...statusMessages.value,
  ].slice(0, MESSAGE_LIMIT)
}

function clearProofState(): void {
  proofState.value = null
  proofLoading.value = false
}

watch(baseUrl, (value) => {
  if (typeof window !== 'undefined') {
    window.localStorage.setItem('mpt-display-base-url', normalizeBaseUrl(value))
  }
})

async function requestJson<T>(path: string, init?: RequestInit): Promise<T> {
  const url = `${normalizeBaseUrl(baseUrl.value)}${path}`

  try {
    const response = await fetch(url, init)
    connectionStatus.value = 'connected'

    const text = await response.text()
    const payload = text ? JSON.parse(text) : {}

    if (!response.ok) {
      throw new Error(payload.error || `Request failed with status ${response.status}`)
    }

    return payload as T
  }
  catch (error) {
    if (error instanceof SyntaxError) {
      throw new Error('The server returned invalid JSON.')
    }

    if (error instanceof Error) {
      if (/Failed to fetch/i.test(error.message)) {
        connectionStatus.value = 'offline'
      }
      throw error
    }

    connectionStatus.value = 'offline'
    throw new Error('Unknown network error')
  }
}

function postJson<T>(path: string, payload: Record<string, string>): Promise<T> {
  return requestJson<T>(path, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(payload),
  })
}

async function refreshSnapshot(options?: { keepProof?: boolean, skipStatus?: boolean }): Promise<void> {
  refreshing.value = true

  try {
    const [rootResponse, entriesResponse, treeResponse] = await Promise.all([
      requestJson<RootResponse>('/root'),
      requestJson<EntriesResponse>('/entries'),
      requestJson<TreeResponse>('/tree'),
    ])

    rootHash.value = rootResponse.root
    entries.value = entriesResponse.entries
    tree.value = treeResponse.tree

    if (!options?.keepProof) {
      clearProofState()
    }

    if (!options?.skipStatus) {
      pushStatus(
        'info',
        'Snapshot refreshed',
        'Loaded the current root hash, entry list, and recursive trie JSON.',
      )
    }
  }
  finally {
    refreshing.value = false
  }
}

async function connectToServer(options?: { skipStatus?: boolean }): Promise<void> {
  connectionStatus.value = 'checking'

  try {
    const health = await requestJson<HealthResponse>('/health')
    if (health.status !== 'ok') {
      throw new Error('The server did not report a healthy status.')
    }

    await refreshSnapshot({ keepProof: true, skipStatus: true })

    if (!options?.skipStatus) {
      pushStatus('success', 'Server reachable', `Connected to ${normalizeBaseUrl(baseUrl.value)}.`)
    }
  }
  catch (error) {
    connectionStatus.value = 'offline'
    const detail = error instanceof Error ? error.message : 'Unable to reach the server.'
    pushStatus('error', 'Connection failed', detail)
    throw error
  }
}

async function insertEntry(): Promise<void> {
  if (!keyInput.value) {
    pushStatus('error', 'Insert blocked', 'A key is required before inserting into the trie.')
    return
  }

  mutating.value = true

  try {
    const response = await postJson<MutationResponse>('/put', {
      key: keyInput.value,
      value: valueInput.value,
    })

    await refreshSnapshot({ skipStatus: true })
    pushStatus('success', 'Entry inserted', `Stored "${keyInput.value}" and updated root ${response.root}.`)
  }
  catch (error) {
    pushStatus('error', 'Insert failed', error instanceof Error ? error.message : 'Unexpected error')
  }
  finally {
    mutating.value = false
  }
}

async function deleteEntry(): Promise<void> {
  if (!keyInput.value) {
    pushStatus('error', 'Delete blocked', 'Choose a key before deleting from the trie.')
    return
  }

  mutating.value = true

  try {
    const response = await postJson<MutationResponse>('/delete', {
      key: keyInput.value,
    })

    await refreshSnapshot({ skipStatus: true })
    if (response.deleted) {
      pushStatus('success', 'Entry deleted', `Removed "${keyInput.value}" and refreshed the trie view.`)
    }
    else {
      pushStatus('info', 'Nothing deleted', `The key "${keyInput.value}" was not present in the trie.`)
    }
  }
  catch (error) {
    pushStatus('error', 'Delete failed', error instanceof Error ? error.message : 'Unexpected error')
  }
  finally {
    mutating.value = false
  }
}

async function loadDemo(): Promise<void> {
  mutating.value = true

  try {
    const response = await postJson<MutationResponse>('/demo', {})
    await refreshSnapshot({ skipStatus: true })
    pushStatus('success', 'Demo loaded', `Loaded ${response.count ?? 0} sample entries into the trie.`)
  }
  catch (error) {
    pushStatus('error', 'Demo load failed', error instanceof Error ? error.message : 'Unexpected error')
  }
  finally {
    mutating.value = false
  }
}

async function resetTrie(): Promise<void> {
  mutating.value = true

  try {
    await postJson<MutationResponse>('/reset', {})
    keyInput.value = ''
    valueInput.value = ''
    await refreshSnapshot({ skipStatus: true })
    pushStatus('success', 'Trie reset', 'Cleared every entry and reset the visualization.')
  }
  catch (error) {
    pushStatus('error', 'Reset failed', error instanceof Error ? error.message : 'Unexpected error')
  }
  finally {
    mutating.value = false
  }
}

function setProofState(source: 'lookup' | 'proof', key: string, value: string, payload: GetResponse | ProofResponse): void {
  proofState.value = {
    source,
    key,
    found: payload.found,
    value,
    proofIds: payload.proof_ids ?? [],
    proofLength: 'proof_length' in payload ? (payload.proof_length ?? payload.proof_ids?.length ?? 0) : (payload.proof_ids?.length ?? 0),
    verified: 'verified' in payload ? payload.verified ?? null : null,
    nodes: 'nodes' in payload ? payload.nodes ?? [] : [],
  }
}

async function enrichProofDetails(key: string): Promise<void> {
  proofLoading.value = true

  try {
    const proof = await postJson<ProofResponse>('/proof', { key })
    if (!proof.found) {
      return
    }

    setProofState('lookup', key, proof.value ?? '', proof)
  }
  catch (error) {
    pushStatus(
      'info',
      'Proof details unavailable',
      error instanceof Error ? error.message : 'Lookup succeeded, but proof details could not be loaded.',
    )
  }
  finally {
    proofLoading.value = false
  }
}

async function lookupEntry(): Promise<void> {
  if (!keyInput.value) {
    pushStatus('error', 'Lookup blocked', 'Enter a key before requesting a lookup.')
    return
  }

  querying.value = true

  try {
    const response = await postJson<GetResponse>('/get', {
      key: keyInput.value,
    })

    if (!response.found) {
      proofState.value = {
        source: 'lookup',
        key: keyInput.value,
        found: false,
        value: '',
        proofIds: [],
        proofLength: 0,
        verified: null,
        nodes: [],
      }
      valueInput.value = ''
      pushStatus('info', 'Lookup missed', `The trie does not contain "${keyInput.value}".`)
      return
    }

    const value = response.value ?? ''
    valueInput.value = value
    setProofState('lookup', keyInput.value, value, response)
    pushStatus('success', 'Lookup succeeded', `Found "${keyInput.value}" with value "${value}".`)
    await enrichProofDetails(keyInput.value)
  }
  catch (error) {
    pushStatus('error', 'Lookup failed', error instanceof Error ? error.message : 'Unexpected error')
  }
  finally {
    querying.value = false
  }
}

async function requestProof(): Promise<void> {
  if (!keyInput.value) {
    pushStatus('error', 'Proof blocked', 'Enter a key before generating a proof.')
    return
  }

  querying.value = true
  proofLoading.value = true

  try {
    const response = await postJson<ProofResponse>('/proof', {
      key: keyInput.value,
    })

    if (!response.found) {
      proofState.value = {
        source: 'proof',
        key: keyInput.value,
        found: false,
        value: '',
        proofIds: [],
        proofLength: 0,
        verified: null,
        nodes: [],
      }
      valueInput.value = ''
      pushStatus('info', 'Proof not generated', `No proof exists because "${keyInput.value}" is not in the trie.`)
      return
    }

    const value = response.value ?? ''
    valueInput.value = value
    setProofState('proof', keyInput.value, value, response)
    pushStatus(
      'success',
      'Proof generated',
      `Collected ${response.proof_length ?? response.nodes?.length ?? 0} proof nodes for "${keyInput.value}".`,
    )
  }
  catch (error) {
    pushStatus('error', 'Proof failed', error instanceof Error ? error.message : 'Unexpected error')
  }
  finally {
    querying.value = false
    proofLoading.value = false
  }
}

function chooseEntry(entry: EntryRecord): void {
  keyInput.value = entry.key
  valueInput.value = entry.value
}

async function reconnect(): Promise<void> {
  try {
    await connectToServer()
  }
  catch {
    // Error state is already reported in the status panel.
  }
}

async function manualRefresh(): Promise<void> {
  try {
    await refreshSnapshot({ keepProof: true })
  }
  catch (error) {
    pushStatus('error', 'Refresh failed', error instanceof Error ? error.message : 'Unexpected error')
  }
}

onMounted(async () => {
  try {
    await connectToServer({ skipStatus: true })
  }
  catch {
    // Initial connection failures are surfaced in the status panel.
  }
})
</script>

<template>
  <div class="app-shell">
    <header class="hero">
      <div class="hero-copy">
        <p class="eyebrow">
          Merkle Patricia Trie Visualizer
        </p>
        <h1>Inspect trie mutations, proofs, and structure in one Vue dashboard.</h1>
        <p class="hero-text">
          This frontend talks directly to the demo C++ server, reloads the current root and entries, and turns the recursive `/tree` payload into an explorable graph.
        </p>
      </div>

      <div class="hero-card">
        <div class="status-row">
          <div class="status-pill" :class="connectionToneClass(connectionStatus)">
            <span class="status-dot" />
            {{ connectionLabel }}
          </div>
          <button class="ghost-button" type="button" :disabled="refreshing" @click="reconnect">
            Reconnect
          </button>
        </div>

        <label class="field-label" for="base-url">Server base URL</label>
        <div class="base-url-row">
          <input
            id="base-url"
            v-model="baseUrl"
            class="text-input monospace"
            type="url"
            placeholder="http://localhost:8080"
          >
          <button class="secondary-button" type="button" :disabled="refreshing" @click="manualRefresh">
            Refresh State
          </button>
        </div>

        <div class="root-card">
          <span class="field-label">Current state root</span>
          <code class="root-hash">{{ rootPreview }}</code>
        </div>
      </div>
    </header>

    <main class="dashboard">
      <section class="panel control-panel">
        <div class="panel-heading">
          <div>
            <p class="panel-kicker">
              Control panel
            </p>
            <h2>Operate on the trie</h2>
          </div>
        </div>

        <div class="form-grid">
          <label class="form-field">
            <span>Key</span>
            <input v-model="keyInput" class="text-input" type="text" placeholder="dog">
          </label>
          <label class="form-field">
            <span>Value</span>
            <input v-model="valueInput" class="text-input" type="text" placeholder="puppy">
          </label>
        </div>

        <div class="button-grid">
          <button class="primary-button" type="button" :disabled="mutating || querying" @click="insertEntry">
            INSERT
          </button>
          <button class="secondary-button" type="button" :disabled="mutating || querying" @click="lookupEntry">
            LOOKUP
          </button>
          <button class="secondary-button" type="button" :disabled="mutating || querying" @click="deleteEntry">
            DELETE
          </button>
          <button class="secondary-button" type="button" :disabled="mutating || querying" @click="requestProof">
            PROOF
          </button>
          <button class="secondary-button" type="button" :disabled="mutating || querying" @click="loadDemo">
            LOAD DEMO
          </button>
          <button class="danger-button" type="button" :disabled="mutating || querying" @click="resetTrie">
            RESET
          </button>
        </div>
      </section>

      <section class="panel proof-panel">
        <div class="panel-heading">
          <div>
            <p class="panel-kicker">
              Proof details
            </p>
            <h2>Highlighted path</h2>
          </div>
          <div v-if="proofLoading" class="small-badge">
            Loading…
          </div>
        </div>

        <div v-if="proofState" class="proof-content">
          <div class="proof-summary-row">
            <div class="summary-chip">
              Source: {{ proofState.source.toUpperCase() }}
            </div>
            <div class="summary-chip">
              {{ proofState.found ? `${proofState.proofLength} nodes` : 'Not found' }}
            </div>
            <div class="summary-chip" :class="{ 'verified-chip': proofState.verified === true, 'failed-chip': proofState.verified === false }">
              {{
                proofState.verified === true
                  ? 'Verified'
                  : proofState.verified === false
                    ? 'Verification failed'
                    : 'Verification not requested'
              }}
            </div>
          </div>

          <dl class="proof-meta">
            <div>
              <dt>Key</dt>
              <dd>{{ proofState.key }}</dd>
            </div>
            <div>
              <dt>Value</dt>
              <dd>{{ proofState.value || '—' }}</dd>
            </div>
          </dl>

          <div class="proof-id-list">
            <span class="field-label">Proof node IDs</span>
            <div class="chip-row">
              <span v-if="proofState.proofIds.length === 0" class="muted-text">No highlighted nodes yet.</span>
              <span v-for="id in proofState.proofIds" :key="id" class="id-chip">
                #{{ id }}
              </span>
            </div>
          </div>

          <div class="proof-nodes">
            <div class="proof-node-header">
              <span>Merkle proof nodes</span>
              <span>{{ proofState.nodes.length }}</span>
            </div>
            <div v-if="proofState.nodes.length === 0" class="muted-text">
              {{
                proofLoading
                  ? 'Fetching node hashes and RLP sizes from /proof…'
                  : 'Run PROOF to inspect node hashes and verification details.'
              }}
            </div>
            <ul v-else class="proof-node-list">
              <li v-for="node in proofState.nodes" :key="`${node.index}-${node.hash}`" class="proof-node-item">
                <div class="proof-node-top">
                  <strong>Node {{ node.index }}</strong>
                  <span>{{ node.size }} bytes</span>
                </div>
                <code class="proof-hash">{{ node.hash }}</code>
              </li>
            </ul>
          </div>
        </div>

        <div v-else class="empty-panel-state">
          Run LOOKUP or PROOF to highlight a path and inspect the related Merkle proof data.
        </div>
      </section>

      <section class="panel status-panel">
        <div class="panel-heading">
          <div>
            <p class="panel-kicker">
              Status
            </p>
            <h2>Operation feedback</h2>
          </div>
        </div>

        <div class="status-feed">
          <article
            v-for="message in statusMessages"
            :key="message.id"
            class="status-message"
            :class="statusToneClass(message.tone)"
          >
            <strong>{{ message.title }}</strong>
            <p>{{ message.detail }}</p>
          </article>
          <p v-if="statusMessages.length === 0" class="muted-text">
            No operations yet. Connect to the server or load the demo dataset to get started.
          </p>
        </div>
      </section>

      <section class="panel entries-panel">
        <div class="panel-heading">
          <div>
            <p class="panel-kicker">
              Entries
            </p>
            <h2>Current key/value pairs</h2>
          </div>
          <div class="small-badge">
            {{ entries.length }}
          </div>
        </div>

        <div class="entries-list">
          <button
            v-for="entry in entries"
            :key="entry.key"
            class="entry-row"
            :class="{ selected: keyInput === entry.key }"
            type="button"
            @click="chooseEntry(entry)"
          >
            <span class="entry-key">{{ entry.key }}</span>
            <span class="entry-value">{{ entry.value }}</span>
          </button>
          <p v-if="entries.length === 0" class="muted-text">
            No entries stored yet.
          </p>
        </div>
      </section>

      <section class="panel tree-panel">
        <div class="panel-heading">
          <div>
            <p class="panel-kicker">
              Tree visualization
            </p>
            <h2>Recursive trie graph</h2>
          </div>
          <div class="legend">
            <span class="legend-chip leaf">LEAF</span>
            <span class="legend-chip ext">EXT</span>
            <span class="legend-chip branch">BRANCH</span>
          </div>
        </div>

        <TrieGraph :tree="tree" :highlighted-node-ids="highlightedNodeIds" />
      </section>
    </main>
  </div>
</template>

<style scoped>
.app-shell {
  display: grid;
  gap: 1.6rem;
}

.hero {
  display: grid;
  grid-template-columns: minmax(0, 1.25fr) minmax(320px, 0.95fr);
  gap: 1.4rem;
  align-items: stretch;
}

.hero-copy,
.hero-card,
.panel {
  position: relative;
  overflow: hidden;
  border-radius: 30px;
  border: 1px solid rgba(16, 42, 67, 0.1);
  background: rgba(255, 251, 244, 0.84);
  box-shadow:
    0 26px 60px rgba(21, 44, 75, 0.08),
    inset 0 1px 0 rgba(255, 255, 255, 0.72);
}

.hero-copy {
  padding: 2.2rem;
  background:
    radial-gradient(circle at top left, rgba(255, 255, 255, 0.95), rgba(244, 233, 211, 0.9)),
    linear-gradient(135deg, rgba(255, 245, 228, 0.92), rgba(234, 244, 255, 0.88));
}

.hero-copy::after,
.hero-card::after,
.panel::after {
  content: '';
  position: absolute;
  inset: auto -10% -35% auto;
  width: 180px;
  height: 180px;
  border-radius: 50%;
  background: radial-gradient(circle, rgba(220, 123, 42, 0.16), transparent 68%);
  pointer-events: none;
}

.hero-card {
  padding: 1.5rem;
  display: grid;
  gap: 1rem;
  background:
    linear-gradient(180deg, rgba(248, 252, 255, 0.95), rgba(235, 243, 255, 0.88)),
    radial-gradient(circle at top right, rgba(87, 144, 255, 0.14), transparent 58%);
}

.eyebrow,
.panel-kicker {
  text-transform: uppercase;
  letter-spacing: 0.13em;
  font-size: 0.78rem;
  font-weight: 800;
  color: #ad5a1f;
}

.hero h1,
.panel h2 {
  color: #16243b;
  font-weight: 800;
  letter-spacing: -0.03em;
}

.hero h1 {
  margin-top: 0.5rem;
  font-size: clamp(2.1rem, 4vw, 3.6rem);
  line-height: 1.02;
  max-width: 14ch;
}

.hero-text {
  margin-top: 1rem;
  max-width: 60ch;
  color: rgba(29, 49, 78, 0.78);
  font-size: 1rem;
}

.status-row,
.base-url-row,
.panel-heading,
.proof-summary-row,
.proof-node-top {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.8rem;
}

.base-url-row {
  align-items: stretch;
}

.status-pill,
.small-badge,
.summary-chip,
.legend-chip,
.id-chip {
  display: inline-flex;
  align-items: center;
  gap: 0.45rem;
  border-radius: 999px;
  padding: 0.5rem 0.85rem;
  font-size: 0.84rem;
  font-weight: 800;
  letter-spacing: 0.04em;
}

.status-pill {
  background: rgba(255, 255, 255, 0.78);
  border: 1px solid rgba(16, 42, 67, 0.1);
  color: #1a3554;
}

.status-dot {
  width: 0.68rem;
  height: 0.68rem;
  border-radius: 50%;
  background: currentColor;
  box-shadow: 0 0 0 4px rgba(255, 255, 255, 0.45);
}

.status-connected {
  color: #2f8f58;
}

.status-checking {
  color: #aa6b1f;
}

.status-offline {
  color: #bf4037;
}

.field-label {
  display: block;
  font-size: 0.82rem;
  font-weight: 800;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: rgba(24, 47, 75, 0.68);
}

.root-card {
  display: grid;
  gap: 0.45rem;
  padding: 1rem;
  border-radius: 22px;
  background: rgba(255, 255, 255, 0.7);
  border: 1px solid rgba(16, 42, 67, 0.08);
}

.root-hash,
.proof-hash,
.monospace {
  font-family:
    'SFMono-Regular',
    Consolas,
    'Liberation Mono',
    Menlo,
    monospace;
}

.root-hash,
.proof-hash {
  display: block;
  overflow-wrap: anywhere;
  color: #173558;
  font-size: 0.92rem;
}

.dashboard {
  display: grid;
  grid-template-columns: minmax(320px, 430px) minmax(0, 1fr);
  gap: 1.35rem;
  align-items: start;
}

.dashboard > :nth-child(1),
.dashboard > :nth-child(2),
.dashboard > :nth-child(3),
.dashboard > :nth-child(4) {
  grid-column: 1;
}

.tree-panel {
  grid-column: 2;
  grid-row: 1 / span 4;
  display: grid;
  grid-template-rows: auto minmax(0, 1fr);
  align-self: start;
  --graph-height: clamp(560px, 82vh, 1100px);
  --graph-max-height: min(180vh, 1400px);
}

.panel {
  padding: 1.35rem;
}

.panel-heading {
  margin-bottom: 1rem;
}

.form-grid {
  display: grid;
  gap: 0.9rem;
}

.form-field {
  display: grid;
  gap: 0.38rem;
  color: #1a3554;
  font-weight: 700;
}

.text-input {
  width: 100%;
  border: 1px solid rgba(16, 42, 67, 0.14);
  border-radius: 18px;
  background: rgba(255, 255, 255, 0.92);
  color: #132844;
  padding: 0.85rem 1rem;
  font: inherit;
  transition:
    border-color 160ms ease,
    box-shadow 160ms ease,
    transform 160ms ease;
}

.text-input:focus {
  outline: none;
  border-color: rgba(41, 86, 165, 0.4);
  box-shadow: 0 0 0 4px rgba(89, 136, 219, 0.14);
}

.button-grid {
  margin-top: 1rem;
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 0.75rem;
}

.primary-button,
.secondary-button,
.danger-button,
.ghost-button {
  border: none;
  border-radius: 18px;
  padding: 0.82rem 1rem;
  font: inherit;
  font-weight: 800;
  letter-spacing: 0.04em;
  cursor: pointer;
  transition:
    transform 140ms ease,
    filter 140ms ease,
    opacity 140ms ease;
}

.primary-button:hover,
.secondary-button:hover,
.danger-button:hover,
.ghost-button:hover {
  transform: translateY(-1px);
  filter: brightness(1.02);
}

.primary-button:disabled,
.secondary-button:disabled,
.danger-button:disabled,
.ghost-button:disabled {
  cursor: not-allowed;
  opacity: 0.55;
  transform: none;
}

.primary-button {
  background: linear-gradient(135deg, #214c8f, #4271bd);
  color: #fffdf8;
}

.secondary-button,
.ghost-button {
  background: rgba(255, 255, 255, 0.9);
  color: #173558;
  border: 1px solid rgba(16, 42, 67, 0.12);
}

.danger-button {
  background: linear-gradient(135deg, #b7493d, #d46455);
  color: #fff8f5;
}

.proof-panel,
.status-panel,
.entries-panel {
  display: grid;
}

.proof-content {
  display: grid;
  gap: 1rem;
}

.summary-chip,
.small-badge,
.legend-chip,
.id-chip {
  background: rgba(255, 255, 255, 0.78);
  color: #173558;
  border: 1px solid rgba(16, 42, 67, 0.08);
}

.verified-chip {
  color: #2f8f58;
}

.failed-chip {
  color: #bf4037;
}

.proof-meta {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 0.85rem;
}

.proof-meta div {
  padding: 0.9rem;
  border-radius: 20px;
  background: rgba(255, 255, 255, 0.66);
  border: 1px solid rgba(16, 42, 67, 0.08);
}

.proof-meta dt {
  font-size: 0.78rem;
  text-transform: uppercase;
  letter-spacing: 0.09em;
  color: rgba(24, 47, 75, 0.7);
}

.proof-meta dd {
  margin-top: 0.3rem;
  color: #142740;
  font-weight: 700;
}

.chip-row {
  display: flex;
  flex-wrap: wrap;
  gap: 0.55rem;
  margin-top: 0.55rem;
}

.id-chip {
  color: #b33939;
  border-color: rgba(179, 57, 57, 0.18);
  background: rgba(255, 239, 239, 0.92);
}

.proof-nodes {
  display: grid;
  gap: 0.65rem;
}

.proof-node-header {
  display: flex;
  justify-content: space-between;
  gap: 0.8rem;
  align-items: center;
  font-weight: 800;
  color: #193352;
}

.proof-node-list,
.status-feed {
  display: grid;
  gap: 0.75rem;
}

.proof-node-item,
.status-message {
  padding: 0.95rem;
  border-radius: 20px;
  border: 1px solid rgba(16, 42, 67, 0.08);
}

.proof-node-item {
  background: rgba(255, 255, 255, 0.72);
}

.status-message strong {
  display: block;
  color: #142740;
}

.status-message p {
  margin-top: 0.22rem;
  color: rgba(20, 39, 64, 0.82);
}

.message-success {
  background: rgba(238, 248, 241, 0.94);
  border-color: rgba(52, 135, 83, 0.16);
}

.message-error {
  background: rgba(255, 239, 237, 0.96);
  border-color: rgba(191, 64, 55, 0.16);
}

.message-info {
  background: rgba(240, 246, 255, 0.96);
  border-color: rgba(56, 106, 184, 0.14);
}

.entries-list {
  display: grid;
  gap: 0.6rem;
}

.entry-row {
  display: flex;
  justify-content: space-between;
  gap: 0.8rem;
  align-items: center;
  padding: 0.9rem 1rem;
  border-radius: 18px;
  border: 1px solid rgba(16, 42, 67, 0.08);
  background: rgba(255, 255, 255, 0.74);
  text-align: left;
  cursor: pointer;
  transition:
    transform 140ms ease,
    border-color 140ms ease,
    background 140ms ease;
}

.entry-row:hover {
  transform: translateY(-1px);
  border-color: rgba(28, 66, 116, 0.18);
}

.entry-row.selected {
  border-color: rgba(33, 76, 143, 0.28);
  background: rgba(232, 241, 255, 0.96);
}

.entry-key {
  font-weight: 800;
  color: #142740;
}

.entry-value {
  color: rgba(20, 39, 64, 0.74);
}

.legend {
  display: flex;
  flex-wrap: wrap;
  gap: 0.45rem;
}

.leaf {
  background: rgba(79, 184, 116, 0.16);
  color: #2c7d49;
}

.ext {
  background: rgba(243, 155, 66, 0.16);
  color: #aa5c17;
}

.branch {
  background: rgba(138, 77, 255, 0.14);
  color: #5d2fba;
}

.empty-panel-state,
.muted-text {
  color: rgba(24, 47, 75, 0.66);
}

@media (max-width: 1180px) {
  .hero {
    grid-template-columns: 1fr;
  }

  .dashboard {
    grid-template-columns: 1fr;
  }

  .dashboard > :nth-child(1),
  .dashboard > :nth-child(2),
  .dashboard > :nth-child(3),
  .dashboard > :nth-child(4),
  .tree-panel {
    grid-column: auto;
    grid-row: auto;
  }

  .tree-panel {
    --graph-height: clamp(500px, 72vh, 860px);
    --graph-max-height: min(150vh, 1100px);
  }
}

@media (max-width: 720px) {
  .app-shell {
    gap: 1rem;
  }

  .hero-copy,
  .hero-card,
  .panel {
    border-radius: 24px;
  }

  .hero-copy {
    padding: 1.45rem;
  }

  .panel {
    padding: 1rem;
  }

  .base-url-row,
  .panel-heading,
  .status-row,
  .proof-summary-row {
    flex-direction: column;
    align-items: stretch;
  }

  .button-grid,
  .proof-meta {
    grid-template-columns: 1fr;
  }

  .tree-panel {
    --graph-height: clamp(420px, 62vh, 680px);
    --graph-max-height: min(120vh, 760px);
  }

  .entry-row {
    flex-direction: column;
    align-items: flex-start;
  }
}
</style>
