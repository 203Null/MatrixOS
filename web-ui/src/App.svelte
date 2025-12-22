<script>
  import { onMount } from 'svelte'

  const fallbackSize = 8
  const offColor = 'rgb(160, 160, 160)'
  const edgeSlots = Array.from({ length: 8 })
  const baseOverlay = 72
  const glowSoftAlpha = 0.35
  const glowHardAlpha = 0.75
  const highlightBoost = 70
  const highlightAlpha = 0.85
  const highlightMidAlpha = 0.45

  let grid
  let moduleReady = false
  let status = 'Waiting for MatrixOS runtime...'
  let moduleRef = null
  let rafId = 0
  let activePointerId = null
  let fnPointerId = null
  let activeCell = null
  let wasmSignature = null
  let reloadTimer = 0
  let keypadEls = Array(64).fill(null)
  let lastColors = Array(64).fill('')

  const clamp = (value) => Math.max(0, Math.min(255, value))

  const overlayChannel = (value) =>
    Math.round(baseOverlay + (255 - baseOverlay) * (value / 255))

  const applyLedColor = (r, g, b, w) => {
    let rr = clamp(r + w)
    let gg = clamp(g + w)
    let bb = clamp(b + w)
    const isOff = rr === 0 && gg === 0 && bb === 0

    if (!isOff) {
      rr = overlayChannel(rr)
      gg = overlayChannel(gg)
      bb = overlayChannel(bb)
    }

    return { r: rr, g: gg, b: bb, isOff }
  }

  const getCornerClip = (x, y) => {
    switch (x + y * 10) {
      case 43:
        return 'polygon(80% 0, 100% 20%, 100% 100%, 0 100%, 0 0)'
      case 44:
        return 'polygon(20% 0, 100% 0, 100% 100%, 0 100%, 0 20%)'
      case 33:
        return 'polygon(100% 0, 100% 80%, 80% 100%, 0 100%, 0 0)'
      case 34:
        return 'polygon(100% 0, 100% 100%, 20% 100%, 0 80%, 0 0)'
      default:
        return ''
    }
  }

  const renderFrame = () => {
    rafId = requestAnimationFrame(renderFrame)
    if (!moduleReady || !moduleRef || !moduleRef.calledRun) {
      return
    }

    if (
      moduleRef._MatrixOS_Wasm_KeypadTick &&
      (activePointerId !== null || fnPointerId !== null)
    ) {
      moduleRef._MatrixOS_Wasm_KeypadTick()
    }

    const heap = moduleRef.HEAPU8 || window.HEAPU8
    if (!heap || !moduleRef._MatrixOS_Wasm_GetFrameBuffer) {
      return
    }

    const width = moduleRef._MatrixOS_Wasm_GetWidth?.() ?? fallbackSize
    const height = moduleRef._MatrixOS_Wasm_GetHeight?.() ?? fallbackSize
    const byteLength = moduleRef._MatrixOS_Wasm_GetFrameBufferByteLength?.() ?? 0
    const ptr = moduleRef._MatrixOS_Wasm_GetFrameBuffer?.() ?? 0

    if (!ptr || !byteLength) {
      return
    }

    const data = heap.subarray(ptr, ptr + byteLength)
    const count = Math.min(width * height, keypadEls.length)

    for (let i = 0; i < count; i += 1) {
      const base = i * 4
      const { r, g, b, isOff } = applyLedColor(
        data[base],
        data[base + 1],
        data[base + 2],
        data[base + 3]
      )
      const color = isOff ? offColor : `rgb(${r}, ${g}, ${b})`
      if (color === lastColors[i]) {
        continue
      }
      lastColors[i] = color
      const el = keypadEls[i]
      if (!el) {
        continue
      }
      if (isOff) {
        el.style.setProperty('--key-color', offColor)
        el.style.removeProperty('--key-glow')
        el.style.removeProperty('--key-glow-filter')
      } else {
        const hr = clamp(r + highlightBoost)
        const hg = clamp(g + highlightBoost)
        const hb = clamp(b + highlightBoost)
        const glowHard = `rgba(${r}, ${g}, ${b}, ${glowHardAlpha})`
        const glowSoft = `rgba(${r}, ${g}, ${b}, ${glowSoftAlpha})`
        el.style.setProperty('--key-color', color)
        el.style.setProperty(
          '--key-glow',
          `radial-gradient(circle at 50% 50%, rgba(${hr}, ${hg}, ${hb}, ${highlightAlpha}) 0%, rgba(${r}, ${g}, ${b}, ${highlightMidAlpha}) 45%, rgba(${r}, ${g}, ${b}, 0) 70%)`
        )
        el.style.setProperty(
          '--key-glow-filter',
          `drop-shadow(0 0 6px ${glowHard}) drop-shadow(0 0 14px ${glowSoft})`
        )
      }
    }
  }

  const getWasmSignature = (response) =>
    response.headers.get('etag') ||
    response.headers.get('last-modified') ||
    response.headers.get('content-length')

  const checkWasmUpdate = async () => {
    try {
      const response = await fetch('/MatrixOSHost.wasm', {
        method: 'HEAD',
        cache: 'no-store'
      })
      if (!response.ok) {
        return
      }
      const signature = getWasmSignature(response)
      if (!signature) {
        return
      }
      if (wasmSignature && signature !== wasmSignature) {
        console.info('MatrixOS wasm update detected, reloading.')
        window.location.reload()
        return
      }
      wasmSignature = signature
    } catch (error) {
    }
  }

  const getCellFromEvent = (event) => {
    if (!grid || !moduleRef) {
      return null
    }
    const rect = grid.getBoundingClientRect()
    const width = moduleRef._MatrixOS_Wasm_GetWidth?.() ?? fallbackSize
    const height = moduleRef._MatrixOS_Wasm_GetHeight?.() ?? fallbackSize
    const x = Math.floor(((event.clientX - rect.left) / rect.width) * width)
    const y = Math.floor(((event.clientY - rect.top) / rect.height) * height)
    if (x < 0 || y < 0 || x >= width || y >= height) {
      return null
    }
    return { x, y }
  }

  const sendKey = (cell, pressed) => {
    if (!moduleRef?._MatrixOS_Wasm_KeyEvent || !cell) {
      return
    }
    moduleRef._MatrixOS_Wasm_KeyEvent(cell.x, cell.y, pressed ? 1 : 0)
  }

  const sendFnKey = (pressed) => {
    if (!moduleRef?._MatrixOS_Wasm_FnEvent) {
      return
    }
    moduleRef._MatrixOS_Wasm_FnEvent(pressed ? 1 : 0)
  }

  const handlePointerDown = (event) => {
    if (!moduleReady || activePointerId !== null || !grid) {
      return
    }
    const cell = getCellFromEvent(event)
    if (!cell) {
      return
    }
    activePointerId = event.pointerId
    activeCell = cell
    grid.setPointerCapture(event.pointerId)
    sendKey(cell, true)
    event.preventDefault()
  }

  const handleFnPointerDown = (event) => {
    if (!moduleReady || fnPointerId !== null) {
      return
    }
    fnPointerId = event.pointerId
    event.currentTarget.setPointerCapture(event.pointerId)
    sendFnKey(true)
    event.preventDefault()
  }

  const handlePointerMove = (event) => {
    if (!moduleReady || activePointerId !== event.pointerId) {
      return
    }
    const cell = getCellFromEvent(event)
    if (!cell) {
      return
    }
    if (!activeCell || cell.x !== activeCell.x || cell.y !== activeCell.y) {
      if (activeCell) {
        sendKey(activeCell, false)
      }
      activeCell = cell
      sendKey(cell, true)
    }
  }

  const endFnPointer = (event) => {
    if (!moduleReady || fnPointerId !== event.pointerId) {
      return
    }
    sendFnKey(false)
    fnPointerId = null
    event.preventDefault()
  }

  const endPointer = (event) => {
    if (!moduleReady || activePointerId !== event.pointerId) {
      return
    }
    if (activeCell) {
      sendKey(activeCell, false)
    }
    activePointerId = null
    activeCell = null
    event.preventDefault()
  }

  const waitForRuntime = () => {
    const ready = window.MatrixOSRuntimeReady
    if (ready && typeof ready.then === 'function') {
      return ready
    }

    return new Promise((resolve) => {
      if (!moduleRef) {
        resolve()
        return
      }
      if (moduleRef.runtimeReady) {
        resolve()
        return
      }
      const previous = moduleRef.onRuntimeInitialized
      moduleRef.onRuntimeInitialized = () => {
        if (typeof previous === 'function') {
          previous()
        }
        moduleRef.runtimeReady = true
        resolve()
      }
    })
  }

  onMount(() => {
    moduleRef = window.Module ?? null
    if (!moduleRef) {
      status = 'MatrixOS wasm not loaded.'
      return () => {}
    }

    checkWasmUpdate()
    reloadTimer = window.setInterval(checkWasmUpdate, 2000)

    const start = () => {
      if (!moduleRef.runtimeReady && !moduleRef.calledRun) {
        setTimeout(start, 50)
        return
      }
      const heap = moduleRef.HEAPU8 || window.HEAPU8
      if (!heap || !moduleRef._MatrixOS_Wasm_GetFrameBuffer) {
        status = 'Waiting for framebuffer...'
        setTimeout(start, 50)
        return
      }
      moduleReady = true
      status = 'Live framebuffer streaming.'
      lastColors.fill('')
      renderFrame()
    }

    waitForRuntime().then(start)

    return () => {
      if (rafId) {
        cancelAnimationFrame(rafId)
      }
      if (reloadTimer) {
        clearInterval(reloadTimer)
      }
    }
  })
</script>

<svelte:head>
  <title>MatrixOS Web UI</title>
</svelte:head>

<main class="lp-stage">
  <div class="lp">
    <div class="lp-underglow" aria-hidden="true">
      <div class="lp-underglow-row">
        {#each edgeSlots as _, x}
          <div class="lp-underglow-led-parent">
            <div class="lp-underglow-led"></div>
          </div>
        {/each}
      </div>

      <div class="lp-underglow-middle">
        <div class="lp-underglow-column">
          {#each edgeSlots as _, y}
            <div class="lp-underglow-led-parent">
              <div class="lp-underglow-led"></div>
            </div>
          {/each}
        </div>

        <div class="lp-underglow-column">
          {#each edgeSlots as _, y}
            <div class="lp-underglow-led-parent">
              <div class="lp-underglow-led"></div>
            </div>
          {/each}
        </div>
      </div>

      <div class="lp-underglow-row">
        {#each edgeSlots as _, x}
          <div class="lp-underglow-led-parent">
            <div class="lp-underglow-led"></div>
          </div>
        {/each}
      </div>
    </div>

    <div class="lp-border">
      <div
        class="lp-controls"
        bind:this={grid}
        on:pointerdown={handlePointerDown}
        on:pointermove={handlePointerMove}
        on:pointerup={endPointer}
        on:pointercancel={endPointer}
        on:pointerleave={endPointer}
      >
        {#each edgeSlots as _, y}
          <div class="lp-controls-row">
            {#each edgeSlots as _, x}
              <div
                class="lp-btn-parent"
                style={getCornerClip(x, y) ? `clip-path: ${getCornerClip(x, y)};` : ''}
              >
                <div
                  class="lp-normal-btn"
                  bind:this={keypadEls[y * 8 + x]}
                ></div>
              </div>
            {/each}
          </div>
        {/each}
      </div>
    </div>

    <div
      class="lp-center-key"
      role="button"
      aria-label="Function key"
      on:pointerdown={handleFnPointerDown}
      on:pointerup={endFnPointer}
      on:pointercancel={endFnPointer}
      on:pointerleave={endFnPointer}
    ></div>
  </div>
</main>
{#if !moduleReady}
  <div class="lp-status-bar" aria-live="polite">{status}</div>
{/if}
