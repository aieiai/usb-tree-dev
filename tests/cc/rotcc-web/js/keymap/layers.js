'use strict';

/* 每层一个按钮，直接列出 Fn0..Fn3 */
function renderLayers() {
  $kcLayers.textContent = '';
  for (let layer = 0; layer < KC_LAYER_COUNT; layer++) {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = `kc-layer${layer === kcLayerIndex ? ' active' : ''}`;
    btn.textContent = `Fn${layer}`;
    btn.addEventListener('click', () => selectLayer(layer));
    $kcLayers.appendChild(btn);
  }
}

function selectLayer(layer) {
  if (layer === kcLayerIndex) return;
  kcLayerIndex = layer;
  for (const [index, btn] of [...$kcLayers.children].entries()) {
    btn.classList.toggle('active', index === layer);
  }
  kcSelected = null;
  syncKeycaps();
  refreshControls();
  autoReadKeymap();
}

