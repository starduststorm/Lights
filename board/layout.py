#!/Applications/Kicad/kicad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
# -*- coding: utf-8 -*-
import os
import sys
from pathlib import Path
from math import *
import cmath
import argparse
from urllib.request import getproxies

parser = argparse.ArgumentParser()
parser.add_argument('-p', "--path", action='store', required=True, help="Path to kicad_pcb file")
parser.add_argument('-L', "--do-layout", action='store_true', required=False, help="Run main layout")

parser.add_argument('--draw-poly', action='store', help='Draw a polygon with format "sides,radius,layer"')

parser.add_argument('--delete-all-traces', action='store_true', help='Delete All Traces in the pcbnew and then exit')
parser.add_argument('--delete-all-drawings', action='store_true', help='Delete all lines & texts in the pcbnew and then exit')
parser.add_argument('--delete-short-traces', action='store_true', help='Delete traces of zero or very small length and exit')
parser.add_argument('--dry-run', action='store_true', help='Don\'t save results')
parser.add_argument('-v', dest='verbose', action='count', help="Verbose")
parser.add_argument('--skip-traces', action='store_true', help='Don\'t add traces')
parser.add_argument('--hide-pixel-labels', action='store_true', help="For all modules like D*, set reference text to hidden")
args = parser.parse_args()

sys.path.insert(0, "/Applications/Kicad/kicad.app/Contents/Frameworks/python/site-packages/")
sys.path.append("/Library/Python/3.7/site-packages")
sys.path.append("/Library/Frameworks/Python.framework/Versions/3.7/lib/python3.7/site-packages")

import kicad
import pcbnew

from kicad.pcbnew import Board
from kicad.util.point import Point2D

def sign(x):
  return 1 if x >= 0 else -1

def circle_pt(theta, radius):
  return Point(radius*cos(theta), radius*sin(theta))

class Point(object):
  @classmethod
  def fromWxPoint(cls, wxPoint):
    return Point(wxPoint.x / pcbnew.IU_PER_MM, wxPoint.y / pcbnew.IU_PER_MM)

  @classmethod
  def fromComplex(cls, c):
    return Point(c.real, c.imag)

  def __init__(self, arg1, arg2=None):
    if arg2 is not None:
      self.x = arg1
      self.y = arg2
    elif type(arg1) == pcbnew.wxPoint:
      p = Point.fromWxPoint(arg1)
      self.x = p.x
      self.y = p.y
    elif type(arg1) == complex:
      p = Point.fromComplex(arg1)
      self.x = p.x
      self.y = p.y
    else:
      self.x = arg1[0]
      self.y = arg1[1]

  def wxPoint(self):
    return pcbnew.wxPoint(self.x * pcbnew.IU_PER_MM , self.y * pcbnew.IU_PER_MM)
  
  def point2d(self):
    return Point2D(x,y)

  def translate(self, vec):
    self.x += vec.x
    self.y += vec.y

  def translated(self, vec):
    return Point(self.x+vec.x, self.y+vec.y)

  def polar_translated(self, distance, angle):
    vec = distance * cmath.exp(angle * 1j)
    return self.translated(Point.fromComplex(vec))

  def __getattr__(self, attr):
    if attr == "theta":
      if self.x == 0:
        return pi/2 if self.y > 0 else 3*pi/2
      theta = atan(self.y/self.x)
      if self.x > 0 and self.y > 0:
        return theta
      elif self.x > 0 and self.y < 0:
        return 2*pi + theta
      else:
        return pi + theta
    elif attr == "radius":
      return sqrt(self.x**2 + self.y**2)
    else:
      super().__getattr__(attr)
  
  def __setattr__(self, attr, val):
    if attr == "theta":
      r = self.radius
      self.x = r * cos(val)
      self.y = r * sin(val)
    elif attr == "radius":
      theta = self.theta
      self.x = val * cos(theta)
      self.y = val * sin(theta)
    else:
      super().__setattr__(attr, val)

  def __getitem__(self, i):
    if i == 0:
      return self.x
    if i == 1:
      return self.y
    raise IndexError("index out of range")

  def distance_to(self, other):
    other = Point(other)
    return sqrt((self.x - other.x) ** 2 + (self.y - other.y) ** 2)

  def __str__(self):
    return "(%0.2f, %0.2f)" % (self.x, self.y)

  def __add__(self, oth):
    oth = Point(oth)
    return Point(self.x + oth.x, self.y + oth.y)

  def __sub__(self, oth):
    oth = Point(oth)
    return Point(self.x - oth.x, self.y - oth.y)

  def __truediv__(self, scalar):
    return Point(self.x/scalar, self.y/scalar)
  
  def __eq__(self, oth):
    return self.x == oth.x and self.y == oth.y


###############################################################################

  
class KiCadPCB(object):
  def __init__(self, path):
    self.pcb_path = os.path.abspath(path)
    self.board = Board.from_file(self.pcb_path)

    self.numlayers = pcbnew.PCB_LAYER_ID_COUNT

    self.layertable = {}
    for i in range(self.numlayers):
      self.layertable[self.board._obj.GetLayerName(i)] = i

    self.netcodes = self.board._obj.GetNetsByNetcode()
    self.netnames = {}  
    for netcode, net in self.netcodes.items():
      self.netnames[net.GetNetname()] = net

  def save(self):
    print("Saving...")
    backup_path = self.pcb_path + ".layoutbak"
    if os.path.exists(backup_path):
      os.unlink(backup_path)
    os.rename(self.pcb_path, backup_path)
    print("  Backing up '%s' to '%s'..." % (self.pcb_path, backup_path))
    assert self.board._obj.Save(self.pcb_path)
    print("Saved!")

  def deleteEdgeCuts(self):
    self.deleteAllDrawings(layer='Edge.Cuts')

  def deleteAllTraces(self):
    tracks = self.board._obj.GetTracks()
    for t in tracks:
      print("Deleting track {}".format(t))
      self.board._obj.Delete(t)
  
  def deleteAllDrawings(self, layer=None):
    for drawing in self.board._obj.GetDrawings():
        if layer is None or layer == drawing.GetLayerName():
          print("Deleting drawing {}".format(drawing))
          self.board._obj.Delete(drawing)
  
  def deleteShortTraces(self):
    print("FIXME: disabled because it removes vias")
    exit(1)
    tracks = board._obj.GetTracks()
    for t in tracks:
      start = t.GetStart()
      end = t.GetEnd()
      length = sqrt((end.x - start.x)**2 + (end.y - start.y)**2)
      if length < 100: # millionths of an inch
        print("Deleting trace of short length {}in/1000000".format(length))
        board._obj.Delete(t)
      elif args.hide_pixel_labels:
        hid = 0
        for m in board._obj.m_Modules:
          if m.GetReference().startswith('D'):
            if m.Reference().IsVisible():
              m.Reference().SetVisible(False)
              hid+=1
        print("Hid %i reference labels" % hid)

  def clear_tracks_for_module(self, module): 
    for pad in module._obj.Pads():
      pad_position = Point.fromWxPoint(pad.GetPosition())
      tracks = self.board._obj.GetTracks()
      for track in tracks:
        track_start = Point.fromWxPoint(track.GetStart())
        track_end = Point.fromWxPoint(track.GetEnd())
        track_net = track.GetNet()
        thresh = .0001
        d1 = pad_position.distance_to(track_start)
        d2 = pad_position.distance_to(track_end)

        if d1 < thresh or d2 < thresh:

          print("Deleting old track for module %s pad %s net %s" % (module.reference, pad.GetPadName(), track_net.GetNetname()))
          self.board._obj.Delete(track)

          vias = []
          vias.append(self.board._obj.GetViaByPosition(track_start.wxPoint()))
          vias.append(self.board._obj.GetViaByPosition(track_end.wxPoint()))


          # FIXME: Deleting a via crashes inside kicad API
          # should be able to find and delete zero-length traces to get these??

          # print("vias matching track start: %s" % str())
          # print("vias matching track end: %s" % str(board._obj.GetViaByPosition(track_end.wxPoint())))
          # for via in vias:
          #  if via:
          # for via in board.vias:
          #   if track_start.distance_to(via.position) < thresh or track_end.distance_to(via.position) < thresh:
          #    print("Deleting old via connected to track on net %s" % (track_net.GetNetname(), ))
          #    board._obj.Delete(via)

  # Drawing Tools
  def draw_segment(self, start, end, layer='F.Silkscreen', width=0.15):
    line = pcbnew.PCB_SHAPE()
    self.board._obj.Add(line)
    line.SetShape(pcbnew.S_SEGMENT)
    line.SetStart(start.wxPoint())
    line.SetEnd(end.wxPoint())
    print("LINE from (%f, %f) to (%f, %f)" % (start.x, start.y, end.x, end.y))
    line.SetLayer(self.layertable[layer])
    line.SetWidth(int(width * pcbnew.IU_PER_MM))
    return line
  
  def draw_circle(self, center, radius, layer='F.Silkscreen', width=0.15):
      circle = pcbnew.PCB_SHAPE()
      self.board._obj.Add(circle)
      circle.SetShape(pcbnew.SHAPE_T_CIRCLE)
      circle.SetCenter(center.wxPoint())
      circle.SetArcAngleAndEnd(360)
      circle.SetEnd((center + Point(radius,0)).wxPoint()) # Sets radius.. somehow
      circle.SetLayer(self.layertable[layer])
      circle.SetWidth(int(width * pcbnew.IU_PER_MM))
      return circle
  
  # def draw_arc_1(self, center, radius, start_angle, stop_angle,
  #                 layer='F.Silkscreen', width=0.15):
      
  #     print("Drawing arc with center at (%f,%f), radius: %f, angle %f -> %f" % (center.x, center.y, radius, start_angle, stop_angle))
  #     arcStartPoint = radius * cmath.exp(start_angle * 1j)
  #     arcStartPoint = center.translated(Point.fromComplex(arcStartPoint))
  #     print("arcStartPoint %s" % str(arcStartPoint));
  #     angle = stop_angle - start_angle
  #     arc = pcbnew.PCB_ARC(self.board._obj)
  #     self.board._obj.Add(arc)
  #     arc.SetShape(pcbnew.S_ARC)
  #     arc.SetCenter(center.wxPoint())
  #     arc.SetArcStart(arcStartPoint.wxPoint())
  #     arc.SetAngle(angle * 180/pi * 10)
  #     arc.SetLayer(self.layertable[layer])
  #     arc.SetWidth(int(width * pcbnew.IU_PER_MM))
  #     return arc

  # def draw_arc_2(self, center, arcStartPoint, totalAngle,
  #                 layer='F.Silkscreen', width=0.15):
  #     print("Drawing arc with center: (%f, %f), arc point at (%f,%f), total angle %f" % (center.x, center.y, arcStartPoint.x, arcStartPoint.y, totalAngle))
  #     arc = pcbnew.PCB_ARC(self.board._obj)
  #     self.board._obj.Add(arc)
  #     arc.SetShape(pcbnew.S_ARC)
  #     arc.SetCenter(center.wxPoint())
  #     arc.SetArcStart(arcStartPoint.wxPoint())
  #     arc.SetAngle(totalAngle * 180/pi * 10)
  #     arc.SetLayer(self.layertable[layer])
  #     arc.SetWidth(int(width * pcbnew.IU_PER_MM))
  #     return arc

  def add_copper_trace(self, start, end, net):
    track = pcbnew.PCB_TRACK(self.board._obj)
    track.SetStart(start)
    track.SetEnd(end)
    track.SetLayer(self.layertable["F.Cu"])
    track.SetWidth(int(.25 * 10**6))
    track.SetNet(net)
    self.board._obj.Add(track)
    print("  copper from {} to {} on net {}".format(start, end, net.GetNetname()))
    return track

  def add_via(self, position, net):
    via = pcbnew.PCB_VIA(self.board._obj)
    via.SetPosition(position)
    via.SetLayerPair(self.layertable["F.Cu"], self.layertable["B.Cu"])
    via.SetDrill(int(0.254 * pcbnew.IU_PER_MM))
    via.SetWidth(int(0.4064 * pcbnew.IU_PER_MM))
    via.SetNet(net)
    print("Adding via on net %s at %s" % (net.GetNetname(), str(position)))
    self.board._obj.Add(via)


###############################################################################


class PCBLayout(object):
  center = Point(100,100)
  edge_cut_line_thickness = 0.05
  pixel_pad_map = {"1": "3", "6": "4"} # connect SCK and SD* for SK9822-EC20
  pixel_pad_names = {"GND": "2", "+5V": "5"}
  drawPixelLinesOnly = False
  
  ####

  def __init__(self, path):
    self.kicadpcb = KiCadPCB(path)
    self.board = self.kicadpcb.board

  prev_series_pixel = None
  pixel_prototype = None
  series_pixel_count = 1

  def drawSegment(self, start, end, layer='F.Silkscreen', width=0.15):
    if type(start) is tuple:
      start = Point(start[0], start[1])
    if type(end) is tuple:
      end = Point(end[0], end[1])
    self.kicadpcb.draw_segment(self.center+start, self.center+end, layer, width)

  def seriesPixelDiscontinuity(self):
    self.prev_series_pixel = None

  def drawPoly(self, polySpec):
    def regularPolygonVertices(n_sides, radius):
      angle = 2*pi / n_sides
      vertices = []
      for i in range(n_sides):
        theta = angle * i + 2*pi/n_sides/2
        x = radius * cos(theta)
        y = radius * sin(theta)
        vertices.append((x, y))
      return vertices

    sides, radius, layer = polySpec.split(',')
    sides = int(sides)
    radius = float(radius)

    vertices = regularPolygonVertices(sides,radius)
    for i in range(len(vertices)):
      x1,y1 = vertices[i]
      x2,y2 = vertices[i+1-len(vertices)]
      assert(x1!= x2 or y1 != y2)
      self.drawSegment((x1, y1), (x2, y2), layer)

  def placeSeriesPixel(self, point, orientation, allowOverlaps=True):
    reference = "D%i" % self.series_pixel_count
    
    if self.pixel_prototype is None:
      print("Loading pixel footprint...")
      self.pixel_prototype = pcbnew.FootprintLoad(str(Path(__file__).parent.joinpath('kicad_footprints.pretty').resolve()), "LED-SK9822-EC20")
      self.pixel_prototype.SetLayer(self.kicadpcb.layertable["F.Cu"])

    # if self.prev_series_pixel is not None:
    #   self.drawSegment(Point.fromWxPoint(self.prev_series_pixel.GetPosition()), point, width = 0.1)

    absPoint = self.center + point

    if not allowOverlaps:
      overlapThresh = 0.8
      for fp in self.kicadpcb.board._obj.GetFootprints():
        # print(fp, Point(fp.GetPosition()), fp.GetOrientation())
        fpPoint = Point(fp.GetPosition())
        fpOrientation = pi/180 * fp.GetOrientation()/10
        if absPoint.distance_to(fpPoint) < overlapThresh:
          # the pixel overlaps with a pixel already on the board. we'll adjust it to 'flow' more with the new attemped pixel
          newPt = (fpPoint+absPoint)/2
          orientationTweakSign = 1 if abs(fpOrientation%(pi/2) - orientation%(pi/2)) > pi/4 else -1
          newOrientation = fpOrientation + (fpOrientation%(pi/2) - orientation%(pi/2)) / 2 * orientationTweakSign
          print("Pixel", reference, "overlaps", fp.GetReference(), "first", fpPoint, "@", fpOrientation, "second", absPoint, "@", orientation, " => ", newPt, "@", newOrientation)
          
          movedPads = [pad.GetPosition() for pad in fp.Pads()]
          
          fp.SetOrientation(180/pi*10*newOrientation)
          fp.SetPosition(newPt.wxPoint())

          # adjust any traces that were connected to this pixel
          for padNum in range(len(movedPads)):
            for track in self.kicadpcb.board._obj.GetTracks():
              if Point(track.GetStart()).distance_to(movedPads[padNum]) < 0.1:
                track.SetStart(fp.Pads()[padNum].GetPosition())
              elif Point(track.GetEnd()).distance_to(movedPads[padNum]) < 0.1:
                track.SetEnd(fp.Pads()[padNum].GetPosition())
          
          return

    print("Placing pixel %s at point %s (relative center %s) orientation %f" % (reference, point, self.center, orientation));
    pixel = pcbnew.FOOTPRINT(self.pixel_prototype)
    pixel.SetPosition((self.center + point).wxPoint())
    
    pixel.SetReference(reference)
    # module.position = (point + self.center).point2d()
    kicad_orientation = orientation * 180/pi * 10 # kicad stores 10th's of a degree?? *shrug*
    pixel.SetOrientation(kicad_orientation)
    pixel.Reference().SetVisible(False)
    if self.drawPixelLinesOnly:
      if self.prev_series_pixel is not None:
        self.drawSegment(Point.fromWxPoint(self.prev_series_pixel.GetPosition()) - self.center, point, width = 0.35)
    else:
      self.kicadpcb.board._obj.Add(pixel)
    
    self.series_pixel_count += 1

    for pad in pixel.Pads():
      if not args.skip_traces:
        if pad.GetPadName() == self.pixel_pad_names['GND']:
          # draw trace outward from ground
          end = Point(pad.GetPosition()).polar_translated(0.8, -pi/2-orientation)
          self.kicadpcb.add_copper_trace(pad.GetPosition(), end.wxPoint(), pad.GetNet())

          # add a via
          self.kicadpcb.add_via(end.wxPoint(), pad.GetNet())
        elif pad.GetPadName() == self.pixel_pad_names['+5V']:
          def get5VTranceEnd(pad, orientation):
            return Point(pad.GetPosition()).polar_translated(1.0, pi/2-orientation)
          # draw trace outward from 5V
          end = get5VTranceEnd(pad, orientation)
          self.kicadpcb.add_copper_trace(pad.GetPosition(), end.wxPoint(), pad.GetNet())
          # and connect +5V from previous
          if self.prev_series_pixel is not None:
            prevPad = [pad for pad in self.prev_series_pixel.Pads() if pad.GetPadName() == self.pixel_pad_names['+5V']][0]
            prevEnd = get5VTranceEnd(prevPad, self.prev_series_pixel.GetOrientation()/10*pi/180)
            self.kicadpcb.add_copper_trace(prevEnd.wxPoint(), end.wxPoint(), pad.GetNet())

    # Add tracks from the previous pixel, connecting pad 5 to pad 2 and pad 4 to pad 3
    if not args.skip_traces and self.prev_series_pixel is not None:
      for prev_pad in self.prev_series_pixel.Pads():
        if prev_pad.GetPadName() in self.pixel_pad_map:
          # tracks = self.kicadpcb.board._obj.TracksInNet(prev_pad.GetNet().GetNetCode())
          # if tracks:
          #   # skip pad, already has traces
          #   if args.verbose:
          #     print("Skipping pad, already has {} tracks: {}".format(len(tracks), prev_pad.GetPadName()))
          #   continue

          # for net in board._obj.TracksInNet(prev_pad.GetNet().GetNet()):
          #   board._obj.Delete(t)

          # then connect the two pads
          for pad in pixel.Pads():
            print("    pad name:", pad.GetPadName())
            if pad.GetPadName() == self.pixel_pad_map[prev_pad.GetPadName()]:
              start = prev_pad.GetPosition()
              end = pad.GetPosition()
              print("Adding track from pixel {} pad {} to pixel {} pad {}".format(self.prev_series_pixel.GetReference(), prev_pad.GetPadName(), pixel.GetReference(), pad.GetPadName()))
              self.kicadpcb.add_copper_trace(start, end, pad.GetNet())

    self.prev_series_pixel = pixel

  def doLayout(self, args):
    if args.delete_all_traces:
      self.kicadpcb.deleteAllTraces()

    if args.delete_all_drawings:
      self.kicadpcb.deleteAllDrawings()

    if args.delete_short_traces:
      self.kicadpcb.deleteShortTraces()
    
    if args.draw_poly:
      self.drawPoly(args.draw_poly)

    if args.do_layout:
      self.kicadpcb.deleteAllDrawings(layer='F.Silkscreen')
      self.drawEdgeCuts()
      self.placePixels()
      self.decorateSilkScreen()

    if not args.dry_run:
      self.kicadpcb.save()
  
  ### to override
  def drawEdgeCuts(self):
    self.kicadpcb.deleteEdgeCuts()
    pass

  def placePixels(self):
    import re
    for fp in self.kicadpcb.board._obj.GetFootprints():
      if re.match("^D\d+$",fp.GetReference()):
        print("Removing old footprint for pixel %s" % fp.GetReference())
        self.kicadpcb.board._obj.Delete(fp)
    
    self.kicadpcb.deleteAllTraces()
  
  def decorateSilkScreen(self):
    pass


###############################################################################


class Timing(object):
  @classmethod
  def linear(cls, time, start, change, duration):
    return start + change * time/duration
  
  @classmethod
  def easeInOutCubic(cls, time, start, change, duration): 
    t = time / (duration/2)
    return change/2*t*t*t + start if t < 1 else change/2*((t-2)*(t-2)*(t-2) + 2) + start;
    
  @classmethod
  def easeOutCubic(cls, time, start, change, duration):
    time = time/duration
    t2 = time-1
    return change*(t2*t2*t2 + 1) + start

  @classmethod
  def easeInCubic(cls, time, start, change, duration):
    time = time/duration
    return change * time*time*time + start

layout = PCBLayout(args.path)    

if __name__ == '__main__':
  layout.doLayout(args)

