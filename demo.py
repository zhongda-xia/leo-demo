#!/usr/bin/env python
# coding: utf-8

from Leo import Constellation, Scenario, genCZML, getGtId, genNdnSIM

import csv
from tqdm import tqdm
from skyfield.api import wgs84

### create ground stations at major cities

# choose several most populated cities
# targets = ['Beijing', 'Shanghai', 'New York-Newark', 'São Paulo']

MAX_CITIES = 10
cityDict = {}

with open('cities.csv', encoding='utf-8') as cityCsv:
    reader = csv.DictReader(cityCsv)
    for row in reader:
        try:
            targets
        except NameError:
            cityDict[row['Urban Agglomeration']] = row
            if len(cityDict) >= MAX_CITIES:
                break
        else:
            if row['Urban Agglomeration'] in targets:
                cityDict[row['Urban Agglomeration']] = row
            if len(cityDict) >= len(targets):
                break

print(cityDict.keys())

gtDict = {} # ground stations indexed by name
for city in tqdm(cityDict):
    gtId = getGtId(city)
    lat =  float(cityDict[city]['Latitude'])
    lon =  float(cityDict[city]['Longitude'])
    gtDict[gtId] = wgs84.latlon(lat, lon)

cons_starlink = Constellation(oh=550, no=24, ns=66, incl=53, el=25)
sc_starlink = Scenario(cons_starlink, gtDict, 'orbit closest lazy')

genCZML(sc_starlink, 'starlink.czml', [('city-Beijing', 'city-São Paulo')])

genNdnSIM(sc_starlink, 'starlink', [('city-Beijing', 'city-São Paulo')])