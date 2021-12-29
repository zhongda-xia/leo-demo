#!/usr/bin/env python
# coding: utf-8


### imports

import math
from itertools import permutations

from tqdm import tqdm, trange

import multiprocessing

import ephem
from skyfield.api import S, EarthSatellite
from skyfield.api import load
from sgp4.api import Satrec, WGS84

import networkx as nx

from czml import czml

from datetime import datetime
from datetime import timedelta
from datetime import timezone

### constellation utilities

KEP_CONS = 3.9861e14
RUNS = 1 # how many orbiting periods to simulate

ts = load.timescale()

def getMeanMotion(orbitHeight):
    global KEP_CONS
    return ((KEP_CONS**(1./3))/(orbitHeight+ephem.earth_radius))**(3./2)*86400/(2*ephem.pi)

def getSatTrack(satnum, meanAnomaly, raan, epoch, ecc, perig, incl, mm, period):
    satrec = Satrec()
    satrec.sgp4init(
        WGS84,           # gravity model
        'i',             # 'a' = old AFSPC mode, 'i' = improved mode
        satnum,          # satnum: Satellite number
        epoch,           # epoch: days since 1949 December 31 00:00 UT
        2.8098e-05,      # bstar: drag coefficient (/earth radii)
        6.969196665e-13, # ndot: ballistic coefficient (revs/day)
        0.0,             # nddot: second derivative of mean motion (revs/day^3)
        ecc,             # ecco: eccentricity
        perig,           # argpo: argument of perigee (radians)
        incl/180.*ephem.pi, # inclo: inclination (radians)
        meanAnomaly,        # mo: mean anomaly (radians)
        2*ephem.pi*mm/24./60, # no_kozai: mean motion (radians/minute)
        raan, # nodeo: right ascension of ascending node (radians)
    )

    sat = EarthSatellite.from_satrec(satrec, ts)
    posList = sat.at(period).position.m # returns list of geocentric (x, y, z) coordinates
    track = []
    for i in range(len(posList[0])):
        track.append(i*60)
        track.append(posList[0][i]) # x
        track.append(posList[1][i]) # y
        track.append(posList[2][i]) # z
    return (sat, track) # (satellite object, position list)

# the maximum distance between an earth surface point and a satellite for that satellite to be visible, given an elevation angle
def getMaxDistance(height, angle):
    r = height+ephem.earth_radius/1000
    h = height*1.0
    arh = (angle+90)/180.0*ephem.pi
    ar = math.asin(r*math.sin(arh)/(r+h))
    return (r+h)*math.sin(ephem.pi-arh-ar)/math.sin(arh)

def getSatId(orbitNum, satNum): # name a satellite
    return 'sat-%d-%d' % (orbitNum, satNum)

def getGtId(city): # name a ground station
    return 'city-%s' % city


### constellation

class Satellite:
    def __init__(self, id, on, sn, obj, track):
        self.id = id
        self.orbitNum = on
        self.satNum = sn
        self.sat = obj
        self.track = track

# e.g., cons_starlink = Constellation(oh=550, no=24, ns=66, incl=53, el=25)
class Constellation:
    global RUNS
    global ts

    def __init__(self, oh, incl, no, ns, el, zigzag=False, half=False): # zigzag: set phasing difference, half: pi constellation, orbits spanning 180 degrees
        self.ORBIT_HEIGHT = oh # in km
        self.NUM_ORBITS = no # options: 40 (for 40_40_53deg), 34 (kuiper_p1), 24 (starlink_p1)
        self.NUM_SATS_PER_ORBIT = ns # options: 40 (for 40_40_53deg), 34 (kuiper_p1), 66 (starlink_p1)
        self.INCLINATION = incl # options: 53 (for 40_40_53deg), 51.9 (kuiper_p1), 53 (starlink_p1)
        self.MEAN_MOTION = getMeanMotion(self.ORBIT_HEIGHT*1000)
        self.ORBIT_PERIOD = int(1./self.MEAN_MOTION*24*60) # in minutes

        self.ELEVATION_ANGLE = el

        self.ECCENTRICITY = 0.001 # Circular orbits
        self.ARG_OF_PERIGEE = 0.0 # Circular orbits

        T0 = ts.utc(1949, 12, 31, 0, 0, 0)
        self.SIM_PERIOD = int(RUNS * self.ORBIT_PERIOD)
        self.PERIOD = ts.utc(2021, 1, 1, 0, range(self.SIM_PERIOD), 0)

        self.MAX_DISTANCE = getMaxDistance(self.ORBIT_HEIGHT, self.ELEVATION_ANGLE)

        # create satellites
        print('Creating satellites...')
        self.satArray = [] # satellite records indexed by orbit and satellite number
        self.satDict = {} # for quick lookup
        for orbitNum in trange(self.NUM_ORBITS):
            self.satArray.append([])
            raanFactor = 2
            if half:
                raanFactor = 1
            raan = raanFactor*ephem.pi*orbitNum/self.NUM_ORBITS
            meanAnomalyOffset = 0
            if zigzag:
                meanAnomalyOffset = (orbitNum%2)/2.0
            for satNum in range(self.NUM_SATS_PER_ORBIT):
                satId = getSatId(orbitNum, satNum)
                meanAnomaly = 2*ephem.pi*(satNum+meanAnomalyOffset)/self.NUM_SATS_PER_ORBIT # simple phasing setting, won't mess with inter-plane ISL that are set according to satellite number
                res = getSatTrack(self.NUM_SATS_PER_ORBIT*orbitNum+satNum, meanAnomaly, raan, self.PERIOD[0]-T0, self.ECCENTRICITY, self.ARG_OF_PERIGEE, self.INCLINATION, self.MEAN_MOTION, self.PERIOD)
                self.satArray[orbitNum].append(Satellite(satId, orbitNum, satNum, res[0], res[1]))
                self.satDict[satId] = self.satArray[orbitNum][satNum]
        
        # generate the inter-satellite topology (with delay) at each epoch (minute), only delay varies
        print('Generating topology snapshots (per minute)...')
        self.snapshots = [] # each graph represents the snapshot of the topology at an epoch
        self.distances = {}
        # ISL delay is set according to the distance between two satellites
        for t in trange(self.SIM_PERIOD):
            G = nx.Graph()
            for orbitNum in range(self.NUM_ORBITS):
                for satNum in range(self.NUM_SATS_PER_ORBIT-1):
                    G.add_edge(self.satArray[orbitNum][satNum].id, self.satArray[orbitNum][satNum+1].id, weight=self.getDistance((orbitNum, satNum), (orbitNum, satNum+1)).at(self.PERIOD[t]).distance().km)
                    if orbitNum < self.NUM_ORBITS-1:
                        G.add_edge(self.satArray[orbitNum][satNum].id, self.satArray[orbitNum+1][satNum].id, weight=self.getDistance((orbitNum, satNum), (orbitNum+1, satNum)).at(self.PERIOD[t]).distance().km)
                    else:
                        G.add_edge(self.satArray[orbitNum][satNum].id, self.satArray[0][satNum].id, weight=self.getDistance((orbitNum, satNum), (0, satNum)).at(self.PERIOD[t]).distance().km)
                G.add_edge(self.satArray[orbitNum][0].id, self.satArray[orbitNum][self.NUM_SATS_PER_ORBIT-1].id, weight=self.getDistance((orbitNum, 0), (orbitNum, self.NUM_SATS_PER_ORBIT-1)).at(self.PERIOD[t]).distance().km)
            self.snapshots.append(G)

    def getDistance(self, satCoord1, satCoord2):
        if not (satCoord1, satCoord2) in self.distances:
            self.distances[(satCoord1, satCoord2)] = self.satArray[satCoord1[0]][satCoord1[1]].sat-self.satArray[satCoord2[0]][satCoord2[1]].sat
        return self.distances[(satCoord1, satCoord2)]

### determine the access satellites at each epoch (each minute)

class Attachment:
    def __init__(self, constellation, gtDict, strategy):
        self.constellation = constellation
        self.gtDict = gtDict
        self.dists = {}
        print('Computing distances from GTs...')
        for gtId in tqdm(self.gtDict):
            self.dists[gtId] = self.computeDists(gtId)

        print('Determining access satellites using "%s" strategy'%strategy)
        dispatcher = {
            'closest active': self.closestActive,
            'closest lazy': self.closestLazy,
            'orbit closest lazy': self.orbitClosestLazy
        }
        self.attachments = dispatcher[strategy]()

    def computeDists(self, gtId):
        dists = {}
        for satId in self.constellation.satDict:
            dists[satId] = self.constellation.satDict[satId].sat-self.gtDict[gtId]
        return dists

    def getClosest(self, gtId, t):
        tSatId = None
        minDist = None
        for satId in self.constellation.satDict:
            dist = self.dists[gtId][satId].at(self.constellation.PERIOD[t]).distance()
            if (dist.km < self.constellation.MAX_DISTANCE and minDist != None and (dist.km < minDist)) or (minDist == None):
                minDist = dist.km
                tSatId = satId
        return tSatId
    
    def getOrbitClosest(self, lastSat, gtId, t):
        orbitNum = self.constellation.satDict[lastSat].orbitNum
        satNum = self.constellation.satDict[lastSat].satNum
        orderedSatNums = []
        if satNum == 0:
            orderedSatNums = [self.constellation.NUM_SATS_PER_ORBIT-1] + list(range(1, self.constellation.NUM_SATS_PER_ORBIT-1))
        elif satNum == self.constellation.NUM_SATS_PER_ORBIT-1:
            orderedSatNums = [satNum-1] + list(range(satNum-2))
        else:
            orderedSatNums = [satNum-1] + list(range(satNum+1, self.constellation.NUM_SATS_PER_ORBIT-1)) + list(range(satNum-1))
        tSatId = None
        minDist = None
        for satNum in orderedSatNums:
            satId = self.constellation.satArray[orbitNum][satNum].id
            dist = self.dists[gtId][satId].at(self.constellation.PERIOD[t]).distance()
            if dist.km < self.constellation.MAX_DISTANCE:
                if (minDist == None) or (dist.km < minDist.km):
                    minDist = dist
                    tSatId = satId
        return tSatId

    # closest active: init->closest, handover->when the closest changes, to->closest
    def closestActive(self):
        attachments = {}
        for gtId in tqdm(self.gtDict):
            gtAttachments = []
            for t in range(self.constellation.SIM_PERIOD):
                gtAttachments.append(self.getClosest(gtId, t))
            attachments[gtId] = gtAttachments
        return attachments

    # closest lazy: init->closest, handover->invisible, to->closest
    def closestLazy(self):
        attachments = {}
        for gtId in tqdm(self.gtDict):
            gtAttachments = []
            for t in range(self.constellation.SIM_PERIOD):
                tSatId = None
                if len(gtAttachments) > 0 and gtAttachments[-1] != None:
                    lastSat = gtAttachments[-1]
                    lastDist = self.dists[gtId][lastSat].at(self.constellation.PERIOD[t]).distance()
                    if lastDist.km < self.constellation.MAX_DISTANCE:
                        tSatId = lastSat
                    else:
                        tSatId = self.getClosest(gtId, t)
                gtAttachments.append(tSatId)
            attachments[gtId] = gtAttachments
        return attachments

    # orbit closest lazy: init->closest, handover->invisible, to->1.closest in same orbit, 2.closest
    def orbitClosestLazy(self):
        attachments = {}
        for gtId in tqdm(self.gtDict):
            gtAttachments = []
            for t in range(self.constellation.SIM_PERIOD):
                tSatId = None
                if len(gtAttachments) > 0 and gtAttachments[-1] != None:
                    lastSat = gtAttachments[-1]
                    lastDist = self.dists[gtId][lastSat].at(self.constellation.PERIOD[t]).distance()
                    if lastDist.km < self.constellation.MAX_DISTANCE:
                        tSatId = lastSat
                    else:
                        tSatId = self.getOrbitClosest(lastSat, gtId, t)
                if tSatId == None:
                    tSatId = self.getClosest(gtId, t)
                gtAttachments.append(tSatId)
            attachments[gtId] = gtAttachments
        return attachments


### scenario related, a scenario binds a constellation to a set of ground stations

# returns global routes at each epoch for each producer
def globalRoutes(args):
    period, snapshots, attachments, gtId = args
    routes = {}
    for t in range(period):
        att = attachments[t]
        G = snapshots[t]
        route = set()
        paths = nx.single_source_dijkstra_path(G, att, weight='weight') # producer as source
        for target in paths:
            for i in range(1, len(paths[target])):
                route.add((paths[target][i], paths[target][i-1]))
        routes[t] = route
    return (gtId, routes)


# returns routes at consumer and producer handover epochs for a pair of GTs
def pairRoutes(args):
    period, snapshots, attachments, gtPair = args
    consumer, producer = gtPair
    routes = {}
    sAtt = attachments[consumer]
    dAtt = attachments[producer]
    lastS = None
    lastD = None
    lastPath = None
    for t in range(period):
        thisS = sAtt[t]
        thisD = dAtt[t]
        G = snapshots[t]
        path = None
        if (thisS != lastS) or (thisD != lastD):
            # when delay is ignored, should produce multiple paths
            # path = nx.all_shortest_paths(G, source=thisS, target=thisD, weight='weight')
            # path = [p for p in path]
            # if len(path) > 1:
            #     print('Multiple path from %s to %s' % (cityPair[0], cityPair[1]))
            # path = path[0]
            path = nx.shortest_path(G, source=thisS, target=thisD, weight='weight') # shortest path considering only delay
            routes[t] = path
            lastPath = path
        else:
            routes[t] = lastPath
        lastS = thisS
        lastD = thisD
    return (gtPair, routes)

class CrossStats:
    def __init__(self, hops, hopsLast, length, hopsBetween, curSat, lastSat):
        self.hops = hops
        self.hopsLast = hopsLast
        self.length = length
        self.hopsBetween = hopsBetween
        self.curSat = curSat
        self.lastSat = lastSat

# returns path overlapping stats at consumer handover epochs for a pair of GTs
def pairCrossStats(args):
    period, snapshots, attachments, gtPair = args
    consumer, producer = gtPair
    stats = {}
    routes = {}
    sAtt = attachments[consumer]
    dAtt = attachments[producer]
    lastS = None
    lastD = None
    lastPath = None
    for t in range(period):
        thisS = sAtt[t]
        thisD = dAtt[t]
        G = snapshots[t]
        path = None
        if (thisS != lastS) or (thisD != lastD):
            path = nx.shortest_path(G, source=thisS, target=thisD, weight='weight') # shortest path considering only delay
            routes[t] = path
        else:
            path = lastPath
        if lastS != None and lastS != thisS: # consumer handover, the first attachment is not a handover
            done = False
            for i in range(len(lastPath)):
                for j in range(len(path)):
                    if path[j] == lastPath[i]:
                        stats[t] = CrossStats(hops=j, hopsLast=i, length=len(path), hopsBetween=len(nx.shortest_path(G, source=thisS, target=lastS, weight='weight'))-1, curSat=thisS, lastSat=lastS)
                        done = True
                        break
                if done:
                    break
            if not done:
                stats[t] = CrossStats(hops=len(path), hopsLast=len(lastPath), length=len(path), hopsBetween=len(nx.shortest_path(G, source=thisS, target=lastS, weight='weight'))-1, curSat=thisS, lastSat=lastS)
        lastS = thisS
        lastD = thisD
        lastPath = path
    return (gtPair, (stats, routes))

# gtDict is a dictionary of geographic locations (wsg84.latlon()) indexed by ground station ID (e.g., city-Beijing)
class Scenario:
    def __init__(self, constellation, gtDict, strategy='', attachments=None):
        self.constellation = constellation
        self.gtDict = gtDict
        if strategy == '':
            self.attachments = attachments # for testing
        else:
            self.attachments = Attachment(self.constellation, self.gtDict, strategy).attachments
        self.store = {}
        self.funcDict = {
            'global routes': globalRoutes,
            'pair routes': pairRoutes,
            'pair cross stats': pairCrossStats
        }

    def getGlobalRoutes(self):
        infoType = 'global routes'
        if infoType not in self.store:
            print('Computing %s...'%infoType)
            args = [(self.constellation.SIM_PERIOD, self.constellation.snapshots, self.attachments[gtId], gtId) for gtId in self.gtDict]
            self.store[infoType] = self.process(self.funcDict[infoType], args)
        return self.store[infoType]

    def getPairRoutes(self):
        infoType = 'pair routes'
        if infoType not in self.store:
            print('Computing %s...'%infoType)
            gtPairs = list(permutations(self.gtDict.keys(), 2))
            args = [(self.constellation.SIM_PERIOD, self.constellation.snapshots, self.attachments, gtPair) for gtPair in gtPairs]
            self.store[infoType] = self.process(self.funcDict[infoType], args)
        return self.store[infoType]

    def getCrossStats(self):
        infoType = 'pair cross stats'
        if infoType not in self.store:
            print('Computing %s...'%infoType)
            gtPairs = list(permutations(self.gtDict.keys(), 2))
            args = [(self.constellation.SIM_PERIOD, self.constellation.snapshots, self.attachments, gtPair) for gtPair in gtPairs]
            print('%d tasks...'%len(args))
            self.store[infoType] = self.process(self.funcDict[infoType], args)
        return self.store[infoType]

    def process(self, func, args):
        cores = multiprocessing.cpu_count()
        pool = multiprocessing.Pool(processes=cores)
        res = {}
        for ret in tqdm(pool.imap_unordered(func, args)):
            res[ret[0]] = ret[1]
        return res


### CZML section

CZML_DIR = 'czml_files/'

### functions for adding entities

def genInterval(start, last, this):
    return '/'.join([(start+timedelta(minutes=last)).isoformat(), (start+timedelta(minutes=this)).isoformat()])

def genPolyline(id1, id2, intervals, color, suffix=''):
    polyId = 'line-%s-%s%s'%(id1, id2, suffix)
    packet = czml.CZMLPacket(id=polyId)
    sc = czml.SolidColor(color=color)
    m = czml.Material(solidColor=sc)
    refs = [id1+'#position', id2+'#position']
    pos = czml.Positions(references=refs)
    poly = czml.Polyline(width=8, followSurface=False, material=m, positions=pos)
    show = []
    avl = []
    for i in range(len(intervals)):
        start, end = intervals[i].split('/')
        show.append({'interval':intervals[i], 'show':True})
        avl.append(intervals[i])
    if len(intervals) == 0:
        show = False
    poly.show = show
    packet.polyline = poly
    packet.availability = avl
    return packet

# create and append the document packet
def initDoc(doc, start, end):
    packetDoc = czml.CZMLPacket(id='document',version='1.0')
    clock = czml.Clock()
    clock.interval = '/'.join([start.isoformat(), end.isoformat()])
    clock.currentTime = start.isoformat()
    clock.multiplier = 60
    clock.range = "LOOP_STOP",
    clock.step = "SYSTEM_CLOCK_MULTIPLIER"
    packetDoc.clock = clock
    doc.packets.append(packetDoc)

# add satellites
def addSats(doc, satDict, start, end):
    print('Adding satellites to CZML file...')
    for satId in tqdm(satDict):
        packet = czml.CZMLPacket(id=satId)
        track = czml.Position()
        track.interpolationAlgorithm = 'LAGRANGE'
        track.interpolationDegree = 5
        track.referenceFrame = 'INERTIAL'
        track.epoch = start.isoformat()
        track.cartesian = satDict[satId].track
        packet.position = track
        sc = czml.SolidColor(color={'rgba': [255, 255, 0, 100]})
        m = czml.Material(solidColor=sc)
        if satDict[satId].satNum == 0:
            path = czml.Path()
            path.material = m
            path.width = 3
            path.show = True
            packet.path = path
        bb = czml.Billboard(scale=1.5, show=True)
        bb.image = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAADJSURBVDhPnZHRDcMgEEMZjVEYpaNklIzSEfLfD4qNnXAJSFWfhO7w2Zc0Tf9QG2rXrEzSUeZLOGm47WoH95x3Hl3jEgilvDgsOQUTqsNl68ezEwn1vae6lceSEEYvvWNT/Rxc4CXQNGadho1NXoJ+9iaqc2xi2xbt23PJCDIB6TQjOC6Bho/sDy3fBQT8PrVhibU7yBFcEPaRxOoeTwbwByCOYf9VGp1BYI1BA+EeHhmfzKbBoJEQwn1yzUZtyspIQUha85MpkNIXB7GizqDEECsAAAAASUVORK5CYII="
        bb.color = {'rgba': [255, 255, 255, 255]}
        packet.billboard = bb
        packet.availability = '/'.join([start.isoformat(), end.isoformat()])
        doc.packets.append(packet)

# add GTs
def addGTs(doc, gtDict, start, end):
    print('Adding GTs to CZML file...')
    for gtId in tqdm(gtDict):
        packet = czml.CZMLPacket(id=gtId)
        pos = czml.Position()
        pos.cartographicRadians = [gtDict[gtId].longitude.degrees/180.*ephem.pi, gtDict[gtId].latitude.degrees/180.*ephem.pi, 0]
        packet.position = pos
        bb = czml.Billboard(scale=1.5, show=True)
        bb.image = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAACvSURBVDhPrZDRDcMgDAU9GqN0lIzijw6SUbJJygUeNQgSqepJTyHG91LVVpwDdfxM3T9TSl1EXZvDwii471fivK73cBFFQNTT/d2KoGpfGOpSIkhUpgUMxq9DFEsWv4IXhlyCnhBFnZcFEEuYqbiUlNwWgMTdrZ3JbQFoEVG53rd8ztG9aPJMnBUQf/VFraBJeWnLS0RfjbKyLJA8FkT5seDYS1Qwyv8t0B/5C2ZmH2/eTGNNBgMmAAAAAElFTkSuQmCC"
        bb.color = {'rgba': [255, 255, 255, 255]}
        packet.billboard = bb
        packet.availability = '/'.join([start.isoformat(), end.isoformat()])
        doc.packets.append(packet)

# add user links
def addUserLinks(doc, satDict, gtDict, attachments, start, period):
    print('Computing link show intervals...')
    intervals = {}
    for gtId in tqdm(attachments):
        lastSat = None
        lastTime = None
        intervals[gtId] = {}
        for satId in satDict:
            intervals[gtId][satId] = []
        for i in range(len(attachments[gtId])):
            if attachments[gtId][i] == None:
                if lastSat != None:
                    intervals[gtId][lastSat].append(genInterval(start, lastTime, i))
                    lastSat = None
                    lastTime = None
                else:
                    continue
            elif attachments[gtId][i] == lastSat:
                continue
            else:
                if lastSat != None:
                    intervals[gtId][lastSat].append(genInterval(start, lastTime, i))
                lastSat = attachments[gtId][i]
                lastTime = i
        if lastSat != None:
            intervals[gtId][lastSat].append(genInterval(start, lastTime, period-1))

    print('Adding user links to CZML file...')
    for gtId in tqdm(gtDict):
        for satId in satDict:
            doc.packets.append(genPolyline(gtId, satId, intervals[gtId][satId], {'rgba': [0, 255, 127, 255]}))

# add routes between two GTs
def addPairRoutes(doc, routes, start, period):
    print('Adding routes between a GT pair...')
    curLinkShow = {}
    lastLinkShow = {}
    lastPath = None
    for epoch in tqdm(routes):
        path = routes[epoch]
        pathLinks = set()
        for i in range(1, len(path)):
            link = (path[i-1], path[i])
            pathLinks.add(link)
            if link not in curLinkShow:
                curLinkShow[link] = [False]*period
            curLinkShow[link][epoch] = True
        if lastPath != None:
            lastPathLinks = set()
            for i in range(1, len(lastPath)):
                link = (lastPath[i-1], lastPath[i])
                lastPathLinks.add(link)
            for link in lastPathLinks-pathLinks:
                if link not in lastLinkShow:
                    lastLinkShow[link] = [False]*period
                lastLinkShow[link][epoch] = True
        lastPath = path

    curRouteIntervals = {}
    for link in tqdm(curLinkShow):
        lastState = False
        lastTime = None
        curRouteIntervals[link] = []
        for i in range(len(curLinkShow[link])):
            if curLinkShow[link][i] == lastState:
                continue
            elif curLinkShow[link][i] == False:
                curRouteIntervals[link].append(genInterval(start, lastTime, i))
                lastState = False
                lastTime = None
            else:
                lastState = curLinkShow[link][i]
                lastTime = i
        if lastState == True:
            curRouteIntervals[link].append(genInterval(start, lastTime, period-1))
        curRouteIntervals[link].sort()
        
    for pair in tqdm(curRouteIntervals):
        doc.packets.append(genPolyline(pair[0], pair[1], curRouteIntervals[pair], {'rgba': [0, 255, 127, 255]}))

    lastRouteIntervals = {}
    for link in tqdm(lastLinkShow):
        lastState = False
        lastTime = None
        lastRouteIntervals[link] = []
        for i in range(len(lastLinkShow[link])):
            if lastLinkShow[link][i] == lastState:
                continue
            elif lastLinkShow[link][i] == False:
                lastRouteIntervals[link].append(genInterval(start, lastTime, i))
                lastState = False
                lastTime = None
            else:
                lastState = lastLinkShow[link][i]
                lastTime = i
        if lastState == True:
            lastRouteIntervals[link].append(genInterval(start, lastTime, period-1))
        lastRouteIntervals[link].sort()
        
    for pair in tqdm(lastRouteIntervals):
        doc.packets.append(genPolyline(pair[0], pair[1], lastRouteIntervals[pair], {'rgba': [255, 0, 0, 255]}, '-last'))

# generate producer routes
def addGlobalRoutes(doc, routes, start):
    intervals = {}
    for epoch in tqdm(routes):
        interval = genInterval(start, epoch, epoch+1)
        route = routes[epoch]
        for link in route:
            if not link in intervals:
                intervals[link] = []
            intervals[link].append(interval)
    for link in tqdm(intervals):
        doc.packets.append(genPolyline(link[0], link[1], intervals[link]))

# generate CZML, in each GT pair, the first element is the consumer
def genCZML(scenario, filename, gtPairs=None):
    global CZML_DIR

    print('Generating CZML file...')

    doc = czml.CZML()

    start = datetime(2021, 1, 1, tzinfo=timezone.utc)
    end = start + timedelta(minutes=scenario.constellation.SIM_PERIOD)

    initDoc(doc, start, end)
    addSats(doc, scenario.constellation.satDict, start, end)
    addGTs(doc, scenario.gtDict, start, end)
    addUserLinks(doc, scenario.constellation.satDict, scenario.gtDict, scenario.attachments, start, scenario.constellation.SIM_PERIOD)

    if gtPairs != None:
        for gtPair in gtPairs:
            addPairRoutes(doc, scenario.getPairRoutes()[gtPair], start, scenario.constellation.SIM_PERIOD)

    doc.write(CZML_DIR+filename)
    print('Done!')


### ndnSIM section, save attachments and routes for ndnSIM

NDNSIM_DIR = 'ndnsim_files/'

def storeNodes(scenario, dir):
    print('Storing nodes...')
    content = ['Name,Type\n']
    for satName in scenario.constellation.satDict:
        line = ','.join([satName, 'Satellite'])
        line += '\n'
        content.append(line)
    for gtName in scenario.gtDict:
        line = ','.join([gtName, 'Station'])
        line += '\n'
        content.append(line)
    nodesFile = open('%snodes.csv'%(dir), 'w')
    nodesFile.writelines(content)
    nodesFile.close()

def storeISLs(scenario, dir):
    print('Storing ISLs...')
    content = ['First,Second\n']
    for edge in scenario.constellation.snapshots[0].edges: # now ISLs are all persistent
        line = ','.join([edge[0], edge[1]])
        line += '\n'
        content.append(line)
    ISLsFile = open('%sISLs.csv'%(dir), 'w')
    ISLsFile.writelines(content)
    ISLsFile.close()

def storeAttachments(scenario, dir):
    print('Storing attachments...')
    for gtId in scenario.attachments:
        content = ['Time,Satellite\n']
        for epoch in range(len(scenario.attachments[gtId])):
            line = None
            satId = scenario.attachments[gtId][epoch]
            if satId == None:
                satId = '-'
            if epoch != 0 and (scenario.attachments[gtId][epoch] == scenario.attachments[gtId][epoch-1]):
                continue
            else:
                line = ','.join([str(epoch), satId])
                line += '\n'
            content.append(line)
        
        attFile = open('%sattachments_%s.csv'%(dir, gtId), 'w')
        attFile.writelines(content)
        attFile.close()

# store GT pairs (consumer, producer) and the routes between each pair
def storeGtPairs(scenario, dir, gtPairs):
    gtPairContent = ['Consumer,Producer\n']
    for gtPair in gtPairs:
        print('Storing pair: %s -> %s...'%gtPair)
        content = ['Time,Route\n']
        route = scenario.getPairRoutes()[gtPair]
        for epoch in route:
            line = '|'.join(route[epoch])
            line = ','.join([str(epoch), line])
            line += '\n'
            content.append(line)
        routeFile = open('%sroutes_%s+%s.csv'%(dir, gtPair[0], gtPair[1]), 'w') # consumer comes first
        routeFile.writelines(content)
        routeFile.close()
        gtPairContent.append(','.join(gtPair)+'\n')
    gtPairFile = open('%spairs.csv'%(dir), 'w')
    gtPairFile.writelines(gtPairContent)
    gtPairFile.close()

def genNdnSIM(scenario, gtPairs):
    global NDNSIM_DIR
    print('Generating ndnSIM files...')
    storeNodes(scenario, NDNSIM_DIR)
    storeISLs(scenario, NDNSIM_DIR)
    storeAttachments(scenario, NDNSIM_DIR)
    storeGtPairs(scenario, NDNSIM_DIR, gtPairs)
    print('Done!')

# # store global routes for a GT (slows down simulation)
# for gtId in gtRoutes:
#     content = ['Time,From,To,Op\n']
#     first = True
#     lastEpoch = None
#     for epoch in gtRoutes[gtId]:
#         if first:
#             for route in gtRoutes[gtId][epoch]:
#                 content.append('%d,%s,%s,add\n'%(epoch, route[0], route[1]))
#             first = False
#         else:
#             add = gtRoutes[gtId][epoch] - gtRoutes[gtId][lastEpoch]
#             remove = gtRoutes[gtId][lastEpoch] - gtRoutes[gtId][epoch]
#             for route in add:
#                 content.append('%d,%s,%s,add\n'%(epoch, route[0], route[1]))
#             for route in remove:
#                 content.append('%d,%s,%s,remove\n'%(epoch, route[0], route[1]))
#         lastEpoch = epoch
#     routeFile = open(outputPath+'/'+'routes_'+gtId+'.csv', 'w')
#     routeFile.writelines(content)
#     routeFile.close()
