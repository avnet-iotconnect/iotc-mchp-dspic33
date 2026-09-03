# media/

Screenshots/photos referenced by the top-level README.md on this branch
(`dspic33ck_mc_starter_kit`). This branch's README differs from main's, so
this file lists what's actually used *here* - see main's `media/README.md`
for that branch's own list.

Already in place (inherited from main, still accurate for this board too -
these show the RNWF11 module itself or generic /IOTCONNECT console UI, not
anything Curiosity-board-specific):

- `jumper-flashing.png` / `jumper-running.png` - RNWF11 power jumper in its `PC3V3`/`HOST3V3` positions, shown
  side by side in Step 4
- `device-page.png` / `templates-button.png` / `create-template-button.png` / `import-button.png` - template-import
  walkthrough in Step 3 (imports main's template as a placeholder - see Step 3's note)
- `create-device-button.png` / `device-name.png` / `select-entity.png` / `template-select.png` / `use-my-cert.png` -
  device-creation walkthrough in Step 5 (`template-select.png` shows main's template - see the TODO in Step 3)

Motor-control-board-specific, already in place:

- `mcsk-product.png` - product photo, shown at the top of the README
- `mcsk-rnwf-connection.png` - the RNWF11 correctly installed in mikroBUS/Click socket B (green check) vs.
  socket A (red X), shown in Step 6
- `mcsk-connections-flash.png` - board connections for flashing (power, PKOB4 USB, RNWF11 already mounted), shown in Step 9
- `mcsk-connections-run.png` - board connections for running the demo (same as flashing, but USB moved from
  PKOB4 to the USB-UART port to view console output), shown in Step 9

Still needed (see the `<!-- TODO -->` comments in README.md for context on each):

- A photo of the starter kit board, for the Hardware prerequisite in Step 1
- A screenshot of the /IOTCONNECT Live Data tab showing this board's telemetry, for Step 9 (needs a matching device template first - see Step 3)

Not currently used on this branch (main-only, kept there):

- `curiosity-board-product.png` / `dim-product.png` - Curiosity board product photos
- `module-install.png` - the DIM installed on the Curiosity board
- `wifi-install.png` - mikroBUS A vs. B photo specific to the Curiosity board's silkscreen labeling
- `live-data.png` - main board's Live Data screenshot (shows `"random"` telemetry, not this board's fields)
- `ipe-steps.png` - MPLAB IPE walkthrough (this branch builds/programs from MPLAB X IDE directly instead)
