import { test, expect } from '@playwright/test'

test('renders the trie dashboard shell', async ({ page }) => {
  await page.route('http://localhost:8080/health', async (route) => {
    await route.fulfill({ json: { status: 'ok' } })
  })
  await page.route('http://localhost:8080/root', async (route) => {
    await route.fulfill({
      json: {
        root: '9a74d99ff587fcf8722954a139497a8f74cfcb4de171d2fa422046557559b2ee',
      },
    })
  })
  await page.route('http://localhost:8080/entries', async (route) => {
    await route.fulfill({
      json: {
        entries: [
          { key: 'dog', value: 'puppy' },
          { key: 'horse', value: 'stallion' },
        ],
      },
    })
  })
  await page.route('http://localhost:8080/tree', async (route) => {
    await route.fulfill({
      json: {
        tree: {
          id: 10,
          type: 'EXT',
          partial: '6',
          child: {
            id: 7,
            type: 'BRANCH',
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
            value: 'verb',
          },
        },
      },
    })
  })

  await page.goto('/')
  await expect(page.getByRole('heading', { name: /Inspect trie mutations/i })).toBeVisible()
  await expect(page.getByText('Connected')).toBeVisible()
  await expect(page.getByText('9a74d99ff587fcf8722954a139497a8f74cfcb4de171d2fa422046557559b2ee')).toBeVisible()
  await expect(page.getByRole('button', { name: 'INSERT' })).toBeVisible()
})
