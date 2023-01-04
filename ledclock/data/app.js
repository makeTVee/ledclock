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
let modeInput;
let timeOffsetInput;
let statusDiv;
let statusFooter;
let statusSpinner;

async function onLoaded() {
  form = document.getElementById("form");
  modeInput = document.getElementById("modeSelect");
  statusDiv = document.getElementById("statusDiv");
  statusFooter = document.getElementById("statusFooter");
  statusFooterSpinner = document.getElementById("statusFooterSpinner");
  statusSpinner = document.getElementById("statusSpinner");
  timeOffsetInput = document.getElementById("timeOffsetInput");

  modeInput.onchange = onModeChange;
  timeOffsetInput.onchange = onTimeOffsetChange;

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

  const { mode, timeOffset } = data;

  modeInput.value = mode;
  timeOffsetInput.value = timeOffset / 60 / 60;

  form.style.display = "block";
  statusDiv.style.display = "none";
  statusFooter.innerHTML = "Ready";
  statusFooterSpinner.style.display = "none";
  statusSpinner.style.display = "none";

  console.log("Ready");
}

async function onModeChange(ev) {
  postValue("Mode", "mode", ev.target.selectedIndex);
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
