// used when hosting the site on the ESP
var address = location.hostname;
var urlBase = "";

if (address.includes("localhost") || address.includes("127.0.0.1")) {
  // used when hosting the site somewhere other than the ESP (handy for testing without waiting forever to upload to SPIFFS)
  address = "192.168.86.55";
  urlBase = `http://${address}`;
}

document.addEventListener("DOMContentLoaded", onLoaded, false);

let form;

let modeSelect;

let paletteSection;
let paletteSelect;
let cyclePaletteOnButton;
let cyclePaletteOffButton;
let paletteDurationInput;

let patternSection;
let patternSelect;
let cyclePatternOnButton;
let cyclePatternOffButton;
let patternDurationInput;

let speedSection;
let speedInput;
let speedInputRange;

let clockSection;
let timeOffsetInput;

let statusDiv;
let statusFooter;
let statusSpinner;

async function onLoaded() {
  statusDiv = document.getElementById("statusDiv");
  statusFooter = document.getElementById("statusFooter");
  statusFooterSpinner = document.getElementById("statusFooterSpinner");
  statusSpinner = document.getElementById("statusSpinner");
  form = document.getElementById("form");

  modeSelect = document.getElementById("modeSelect");
  modeSelect.onchange = onModeChange;

  // pattern controls
  patternSection = document.getElementById("patternSection");
  patternSelect = document.getElementById("patternSelect");
  patternSelect.onchange = onPatternChange;
  cyclePatternOnButton = document.getElementById("cyclePatternOnButton");
  cyclePatternOnButton.onclick = () => onCyclePatternChange(true);
  cyclePatternOffButton = document.getElementById("cyclePatternOffButton");
  cyclePatternOffButton.onclick = () => onCyclePatternChange(false);
  patternDurationInput = document.getElementById("patternDurationInput");
  patternDurationInput.onchange = onPatternDurationChange;

  // speed controls
  speedSection = document.getElementById("speedSection");
  speedInput = document.getElementById("speedInput");
  speedInput.onchange = onSpeedChange;
  speedInputRange = document.getElementById("speedInputRange");
  speedInputRange.onchange = onSpeedChange;

  // palette controls
  paletteSection = document.getElementById("paletteSection");
  paletteSelect = document.getElementById("paletteSelect");
  paletteSelect.onchange = onPaletteChange;
  cyclePaletteOnButton = document.getElementById("cyclePaletteOnButton");
  cyclePaletteOnButton.onclick = () => onCyclePaletteChange(true);
  cyclePaletteOffButton = document.getElementById("cyclePaletteOffButton");
  cyclePaletteOffButton.onclick = () => onCyclePaletteChange(false);
  paletteDurationInput = document.getElementById("paletteDurationInput");
  paletteDurationInput.onchange = onPaletteDurationChange;

  // clock controls
  clockSection = document.getElementById("clockSection");
  timeOffsetInput = document.getElementById("timeOffsetInput");
  timeOffsetInput.onchange = onTimeOffsetChange;

  loadPaletteOptions();
  loadPatternOptions();

  const response = await fetch(`${urlBase}/data`);
  if (!response.ok) {
    console.log({ response });
    statusDiv.innerHTML = `Error getting ${response.url}: ${response.status} ${response.statusText}`;
    statusFooter.innerHTML = "Error";
    statusFooterSpinner.style.display = "none";
    statusSpinner.style.display = "none";
    return;
  }

  const data = await response.json();

  console.log({ data });

  const {
    mode,

    currentPaletteIndex,
    cyclePalette,
    paletteDuration,

    currentPatternIndex,
    cyclePattern,
    patternDuration,

    speed,

    timeOffset,
  } = data;

  modeSelect.value = mode;
  updateModeDisplay(mode);

  patternDurationInput.value = patternDuration;
  patternSelect.value = currentPatternIndex;
  updateCyclePatternButtons(cyclePattern);
  updatePatternDisplay(currentPatternIndex);

  paletteDurationInput.value = paletteDuration;
  paletteSelect.value = currentPaletteIndex;
  updateCyclePaletteButtons(cyclePalette);

  speedInput.value = speed;
  speedInputRange.value = speed;

  timeOffsetInput.value = timeOffset / 60 / 60;

  form.style.display = "block";
  statusDiv.style.display = "none";
  statusFooter.innerHTML = "Ready";
  statusFooterSpinner.style.display = "none";
  statusSpinner.style.display = "none";

  console.log("Ready");
}

async function onModeChange(ev) {
  const mode = ev.target.selectedIndex;
  postValue("Mode", "mode", mode);
  updateModeDisplay(mode);
}

function updateModeDisplay(mode) {
  clockSection.style.display = mode === 1 ? "block" : "none";
  patternSection.style.display = mode === 3 ? "block" : "none";
}

// pattern control event handlers
function updateCyclePatternButtons(cyclePattern) {
  cyclePatternOffButton.className = cyclePattern
    ? "btn btn-outline-secondary"
    : "btn btn-primary";
  cyclePatternOnButton.className = cyclePattern
    ? "btn btn-primary"
    : "btn btn-outline-secondary";
}

async function onCyclePatternChange(cyclePattern) {
  postValue("Cycle Pattern", "cyclePattern", cyclePattern ? 1 : 0);
  updateCyclePatternButtons(cyclePattern);
}

async function onPatternDurationChange(ev) {
  postValue("Pattern Duration", "patternDuration", ev.target.value);
}

async function onPatternChange(ev) {
  const index = ev.target.selectedIndex;
  postValue("Pattern", "currentPatternIndex", index);
  updatePatternDisplay(index);
}

function updatePatternDisplay(index) {
  const pattern = patterns[index];
  paletteSection.style.display = pattern.palettes ? "block" : "none";
  speedSection.style.display = pattern.speed ? "block" : "none";
}

// palette control event handlers
function updateCyclePaletteButtons(cyclePalette) {
  cyclePaletteOffButton.className = cyclePalette
    ? "btn btn-outline-secondary"
    : "btn btn-primary";
  cyclePaletteOnButton.className = cyclePalette
    ? "btn btn-primary"
    : "btn btn-outline-secondary";
}

async function onCyclePaletteChange(cyclePalette) {
  postValue("Cycle Palette", "cyclePalette", cyclePalette ? 1 : 0);
  updateCyclePaletteButtons(cyclePalette);
}

async function onPaletteDurationChange(ev) {
  postValue("Palette Duration", "paletteDuration", ev.target.value);
}

async function onPaletteChange(ev) {
  postValue("Palette", "currentPaletteIndex", ev.target.selectedIndex);
}

async function onSpeedChange(ev) {
  const speed = ev.target.value;
  postValue("Speed", "speed", speed);
  updateSpeedControls(speed);
}

function updateSpeedControls(speed) {
  speedInput.value = speed;
  speedInputRange.value = speed;
}

async function onTimeOffsetChange(ev) {
  postValue("Time Offset", "timeOffset", ev.target.value * 60 * 60);
}

async function postValue(display, name, value) {
  statusFooterSpinner.style.display = "block";
  statusFooter.innerHTML = `Setting ${display}: ${value}`;

  const response = await fetch(`${urlBase}/set?name=${name}&value=${value}`, {
    method: "POST",
  });
  if (!response.ok) {
    console.log({ response });
    statusFooter.innerHTML = `Error setting ${name}: ${response.status} ${response.statusText}`;
    return;
  }

  statusFooterSpinner.style.display = "none";
  statusFooter.innerHTML = `Set ${display}: ${value}`;
}

function loadPaletteOptions() {
  const paletteNames = [
    "Rainbow",
    "Rainbow Stripe",
    "Cloud",
    "Lava",
    "Ocean",
    "Forest",
    "Party",
    "Heat",
    "Ice",
    "Icy Blue",
    "Snow",
    "Red & White",
    "Blue & White",
    "Fairy",
    "Retro C9",
    "Red, Green & White",
    "Holly",
    "Sunset_Real",
    "es_rivendell_15",
    "es_ocean_breeze_036",
    "rgi_15",
    "retro2_16",
    "Analogous_1",
    "es_pinksplash_08",
    "Coral_reef",
    "es_ocean_breeze_068",
    "es_pinksplash_07",
    "es_vintage_01",
    "departure",
    "es_landscape_64",
    "es_landscape_33",
    "rainbowsherbet",
    "gr65_hult",
    "gr64_hult",
    "GMT_drywet",
    "ib_jul01",
    "es_vintage_57",
    "ib15",
    "Fuschia_7",
    "es_emerald_dragon_08",
    "lava",
    "fire",
    "Colorfull",
    "Magenta_Evening",
    "Pink_Purple",
    "es_autumn_19",
    "BlacK_Blue_Magenta_White",
    "BlacK_Magenta_Red",
    "BlacK_Red_Magenta_Yellow",
    "Blue_Cyan_Yellow",
  ];

  paletteNames.forEach((paletteName, index) => {
    const option = document.createElement("option");
    option.value = index;
    option.text = paletteName;
    paletteSelect.add(option);
  });
}

const patterns = [
  { name: "Spin", palettes: true },
  { name: "Solid Palette", palettes: true },
  { name: "Pride" },
  { name: "Color Waves", palettes: true },
  { name: "Rainbow" },
  { name: "Rainbow With Glitter" },
  { name: "Confetti" },
  { name: "Sinelon", speed: true },
  { name: "Juggle", speed: true },
  { name: "BPM", speed: true },
];

function loadPatternOptions() {
  patterns.forEach((pattern, index) => {
    const option = document.createElement("option");
    option.value = index;
    option.text = pattern.name;
    patternSelect.add(option);
  });
}
