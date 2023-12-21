r"""
```bash
$ cd ${KIPRJMOD}
# $ rm -rf output
$ rmdir /s /q output
$ mkdir output
$ kikit separate --source "annotation; ref: MAIN1" dboards-02.kicad_pcb output\main.kicad_pcb
$ kikit separate --source "annotation; ref: POWER1" dboards-02.kicad_pcb output\power.kicad_pcb
$ kikit separate --source "annotation; ref: FLOW1" dboards-02.kicad_pcb output\flow.kicad_pcb
$ python kikit_build.py
```
"""

from typing import List

from kikit import panelize
from kikit.substrate import Substrate
from kikit.units import mm
from kikit.common import findBoardBoundingBox, fromDegrees
from kikit import panelize_ui_impl as ki
import shapely
from shapely.geometry import LineString, box
import pcbnew

import os
from os import path

OUTPUT_DIR = path.join(os.getcwd(), "output")
OUTPUT_PATH = path.join(OUTPUT_DIR, "panel.kicad_pcb")
MAIN_BOARD_PATH = path.join(OUTPUT_DIR, "main.kicad_pcb")
POWER_BOARD_PATH = path.join(OUTPUT_DIR, "power.kicad_pcb")
FLOW_BOARD_PATH = path.join(OUTPUT_DIR, "flow.kicad_pcb")

main_board: pcbnew.BOARD = pcbnew.LoadBoard(MAIN_BOARD_PATH)
power_board: pcbnew.BOARD = pcbnew.LoadBoard(POWER_BOARD_PATH)
flow_board: pcbnew.BOARD = pcbnew.LoadBoard(FLOW_BOARD_PATH)

main_board_size = findBoardBoundingBox(main_board)
power_board_size = findBoardBoundingBox(power_board)
flow_board_size = findBoardBoundingBox(flow_board)

PANEL_ORIGIN = pcbnew.wxPoint(main_board_size.GetWidth() // 2, main_board_size.GetHeight() // 2)
PADDING_X_MM = 5 * mm
PADDING_Y_MM = 5 * mm

panel = panelize.Panel(OUTPUT_PATH)

panel_size = panel.appendBoard(MAIN_BOARD_PATH, PANEL_ORIGIN, tolerance=10*mm, shrink=True)
print("panel size", panel_size.GetWidth(), panel_size.GetHeight())
print("origin", panel_size.GetOrigin()[0])

# Power Board #1
power_origin_1 = panel_size.GetOrigin() + pcbnew.VECTOR2I(
    power_board_size.GetWidth() // 2,
    panel_size.GetHeight()
        + (power_board_size.GetHeight() // 2)
        + PADDING_Y_MM
        # + 10*mm
)
panel.appendBoard(POWER_BOARD_PATH, power_origin_1, tolerance=10*mm, inheritDrc=False)

# Power Board #2
power_origin_2 = panel_size.GetOrigin() + pcbnew.VECTOR2I(
    panel_size.GetWidth()
        - (power_board_size.GetWidth() // 2),
    panel_size.GetHeight()
        + (power_board_size.GetHeight() // 2)
        + PADDING_Y_MM
)
panel.appendBoard(POWER_BOARD_PATH, power_origin_2, tolerance=10*mm, inheritDrc=False)

# Power Board #3
panel_height = main_board_size.GetHeight() \
                + PADDING_Y_MM \
                + power_board_size.GetHeight() \
                + PADDING_Y_MM \
                + flow_board_size.GetWidth()

power_origin_3 = panel_size.GetOrigin() + pcbnew.VECTOR2I(
    power_board_size.GetWidth() // 2, 
    panel_height
        - (power_board_size.GetHeight() // 2)
)
panel.appendBoard(POWER_BOARD_PATH, power_origin_3, tolerance=5*mm, inheritDrc=False)

# Flow Board
flow_origin = panel_size.GetOrigin() + pcbnew.VECTOR2I(
    panel_size.GetWidth()
        - (flow_board_size.GetHeight() // 2),
    panel_size.GetHeight()
        + (power_board_size.GetHeight())
        + PADDING_Y_MM
        + flow_board_size.GetWidth() // 2
        + PADDING_Y_MM
)
panel.appendBoard(FLOW_BOARD_PATH, flow_origin, tolerance=10*mm, inheritDrc=False, rotationAngle=fromDegrees(90))

framing_substrates = ki.dummyFramingSubstrate(panel.substrates, (PADDING_X_MM, PADDING_Y_MM))
_, vCuts, hCuts = panel.makeFrame(PADDING_X_MM, PADDING_X_MM, PADDING_Y_MM)
print('------------------------------')
print(framing_substrates)
panel.buildPartitionLineFromBB(framing_substrates, 15*mm//10)

print('bones: ', panel.backboneLines)
panel.clearTabsAnnotations()
panel.buildTabAnnotationsSpacing(25*mm, 10*mm, 10*mm, framing_substrates)
cuts = panel.buildTabsFromAnnotations(0)
cuts.extend(vCuts)
cuts.extend(hCuts)
print(cuts)
panel.makeMouseBites(cuts, 0.5*mm, 0.75*mm, offset=0)

panel.save()
