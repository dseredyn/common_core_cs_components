#!/usr/bin/env python

# Copyright (c) 2015, Robot Control and Pattern Recognition Group,
# Institute of Control and Computation Engineering
# Warsaw University of Technology
#
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the Warsaw University of Technology nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL <COPYright HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Author: Dawid Seredynski
#

import roslib; roslib.load_manifest('common_core_cs_components')
import sys
import rospy
import copy
import threading

import tf
import xml.dom.minidom as minidom
from visualization_msgs.msg import *
from geometry_msgs.msg import Vector3
import PyKDL
from diagnostic_msgs.msg import *
import tf_conversions.posemath as pm

from ros_utils import marker_publisher

class Geometry(object):
    def __init__(self, type_name):
        self.f = None
        self.type = type_name

class Capsule(Geometry):
    def __init__(self):
        super(Capsule, self).__init__("CAPSULE")
        self.r = None
        self.l = None

class Sphere(Geometry):
    def __init__(self):
        super(Sphere, self).__init__("SPHERE")
        self.r = None

class Link:
    def __init__(self):
        self.idx = None
        self.name = None
        self.geoms = []

class Collision:
    def __init__(self, xml):
        self.parseXml(xml)

    def parseXml(self, xml):
        self.i1 = int(xml.getAttribute("i1"))
        self.i2 = int(xml.getAttribute("i2"))
        self.p1x = float(xml.getAttribute("p1x"))
        self.p1y = float(xml.getAttribute("p1y"))
        self.p1z = float(xml.getAttribute("p1z"))
        self.p2x = float(xml.getAttribute("p2x"))
        self.p2y = float(xml.getAttribute("p2y"))
        self.p2z = float(xml.getAttribute("p2z"))
        self.d = float(xml.getAttribute("d"))
        self.n1x = float(xml.getAttribute("n1x"))
        self.n1y = float(xml.getAttribute("n1y"))
        self.n1z = float(xml.getAttribute("n1z"))
        self.n2x = float(xml.getAttribute("n2x"))
        self.n2y = float(xml.getAttribute("n2y"))
        self.n2z = float(xml.getAttribute("n2z"))

class ColDetVis:
    def __init__(self, subsystem_name, component_name):
        self.pub_marker = marker_publisher.MarkerPublisher('/velma_markers')
        rospy.Subscriber("/" + subsystem_name + "/diag", DiagnosticArray, self.diagCallback)
        self.links = {}
        self.collisions_list = []
        self.component_name = component_name
        self.lock = threading.Lock()
        self.listener = tf.TransformListener()

    def diagCallback(self, data):
        for v in data.status[1].values:
            if v.key == self.component_name:
                dom = minidom.parseString(v.value)
                cd = dom.getElementsByTagName("cd")
                #col_count = cd[0].getAttribute("col_count")
                #print col_count

                cols = cd[0].getElementsByTagName("c")

                with self.lock:
                    self.collisions_list = []
                    for c in cols:
                        self.collisions_list.append( Collision(c) )

                links = cd[0].getElementsByTagName("l")
                for l in links:
                    idx = int(l.getAttribute("idx"))
                    if not idx in self.links:
                        geoms = l.getElementsByTagName("g")
                        link_geoms = []
                        for g in geoms:
                            x = float(g.getAttribute("x"))
                            y = float(g.getAttribute("y"))
                            z = float(g.getAttribute("z"))
                            qx = float(g.getAttribute("qx"))
                            qy = float(g.getAttribute("qy"))
                            qz = float(g.getAttribute("qz"))
                            qw = float(g.getAttribute("qw"))
                            f = PyKDL.Frame(PyKDL.Rotation.Quaternion(qx,qy,qz,qw), PyKDL.Vector(x,y,z))
                            type_name = g.getAttribute("type")
                            if type_name == "SPHERE":
                                gg = Sphere()
                                gg.r = float(g.getAttribute("r"))
                            elif type_name == "CAPSULE":
                                gg = Capsule()
                                gg.r = float(g.getAttribute("r"))
                                gg.l = float(g.getAttribute("l"))
                            else:
                                gg = None
                            if gg:
                                gg.f = f
                                link_geoms.append(gg)
                        with self.lock:
                            self.links[idx] = Link()
                            self.links[idx].idx = idx
                            self.links[idx].name = l.getAttribute("name")
                            self.links[idx].geoms = link_geoms

                break

    def spin(self):
        while not rospy.is_shutdown():
            with self.lock:
                collisions_list = copy.copy(self.collisions_list)
                links = copy.copy(self.links)
            m_id = 0
            if len(collisions_list) > 0:
                print "collisions:"
            for c in collisions_list:
                v1 = PyKDL.Vector(c.p1x, c.p1y, c.p1z)
                v2 = PyKDL.Vector(c.p2x, c.p2y, c.p2z)
                m_id = self.pub_marker.publishVectorMarker(v1, v2, m_id, 1, 0, 0, frame='torso_base', namespace='default', scale=0.01)
                if c.i1 in links and c.i2 in links:
                    print "   ", links[c.i1].name, links[c.i2].name, c.d
            self.pub_marker.eraseMarkers(m_id, 200, frame_id='torso_base', namespace='default')

            m_id = 201
            for idx in links:
                link = links[idx]
                try:
                    pose = self.listener.lookupTransform("world", link.name, rospy.Time(0))
                except:
                    continue
                T_W_L = pm.fromTf(pose)

                for g in link.geoms:
                    if g.type == "SPHERE":
                        m_id = self.pub_marker.publishSinglePointMarker(PyKDL.Vector(), m_id, r=0, g=1, b=0, a=0.5, namespace='collision', frame_id="world", m_type=Marker.SPHERE, scale=Vector3(g.r*2, g.r*2, g.r*2), T=T_W_L*g.f)
                    if g.type == "CAPSULE":
                        m_id = self.pub_marker.publishSinglePointMarker(PyKDL.Vector(0,0,-g.l/2), m_id, r=0, g=1, b=0, a=0.5, namespace='collision', frame_id="world", m_type=Marker.SPHERE, scale=Vector3(g.r*2, g.r*2, g.r*2), T=T_W_L*g.f)
                        m_id = self.pub_marker.publishSinglePointMarker(PyKDL.Vector(0,0,g.l/2), m_id, r=0, g=1, b=0, a=0.5, namespace='collision', frame_id="world", m_type=Marker.SPHERE, scale=Vector3(g.r*2, g.r*2, g.r*2), T=T_W_L*g.f)
                        m_id = self.pub_marker.publishSinglePointMarker(PyKDL.Vector(), m_id, r=0, g=1, b=0, a=0.5, namespace='collision', frame_id="world", m_type=Marker.CYLINDER, scale=Vector3(g.r*2, g.r*2, g.l), T=T_W_L*g.f)
            try:
                rospy.sleep(0.1)
            except:
                pass

if __name__ == "__main__":
    rospy.init_node('col_det_vis', anonymous=True)

    try:
        subsystem_name = rospy.get_param("~subsystem_name")
        component_name = rospy.get_param("~component_name")
    except KeyError as e:
        print "Some ROS parameters are not provided:"
        print e
        exit(1)

    rospy.sleep(0.5)

    cdv = ColDetVis(subsystem_name, component_name)

    cdv.spin()

    exit(0)

