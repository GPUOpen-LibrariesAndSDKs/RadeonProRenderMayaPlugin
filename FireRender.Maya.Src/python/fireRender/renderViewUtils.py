#
# Copyright 2020 Advanced Micro Devices, Inc
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#    http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from PySide import QtCore, QtGui
import shiboken
import maya.OpenMayaUI as omu
import maya.OpenMayaRender as omr
import maya.cmds

filter = None

class Overlay(QtGui.QWidget):
    def __init__(self, parent = None):
        QtGui.QWidget.__init__(self, parent)
        self.setAttribute(QtCore.Qt.WA_NoSystemBackground)
        self.setAttribute(QtCore.Qt.WA_TransparentForMouseEvents)
        self.region = self.rect()
        self.bg = None

    def paintEvent(self, event):
        p = QtGui.QPainter(self)
        if self.bg:
            p.drawPixmap(0,0,self.bg)
        p.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 128), 1, QtCore.Qt.DashLine, QtCore.Qt.RoundCap, QtCore.Qt.RoundJoin))
        p.drawRect(self.region)

class RenderViewEventFilter(QtCore.QObject):
    def __init__(self, widget):
        self.widget = widget
        self.isPressing = False
        self.startPos = 0
        QtCore.QObject.__init__(self)
        self.ov = Overlay(self.widget)

    def eventFilter(self, obj, event):
        if event.type() == QtCore.QEvent.Type.MouseButtonPress:
            if maya.cmds.fireRender(ipr=1,ir=1):
                self.isPressing = True
                self.startPos = event.pos()
                maya.cmds.fireRender(ipr=1,pause=1)
                top = self.widget.mapToGlobal(QtCore.QPoint(0,0))
                desktop = QtGui.QApplication.desktop().winId()
                self.ov.bg = QtGui.QPixmap.grabWindow(desktop,top.x(), top.y(),self.widget.width(),self.widget.height())
                self.ov.setGeometry(self.widget.geometry())
                self.ov.show()
        if event.type() == QtCore.QEvent.Type.MouseButtonRelease:
            if (self.isPressing):
                if maya.cmds.fireRender(ipr=1,ir=1):
                    maya.cmds.fireRender(ipr=1,pause=0)
                self.isPressing = False
                self.ov.hide()

        if event.type() == QtCore.QEvent.Type.MouseMove:
            if (self.isPressing):
                self.ov.region = QtCore.QRect(self.startPos.x(), self.startPos.y(),event.pos().x() - self.startPos.x(),event.pos().y() - self.startPos.y())
                self.ov.repaint()
        return QtCore.QObject.eventFilter(self, obj, event)


def registerRenderViewMarquee():
    ptr = omu.MQtUtil.findControl(maya.cmds.renderWindowEditor("renderView", q=1, ctl=1))
    w = shiboken.wrapInstance(long(ptr), QtGui.QWidget)
    viewportWidget = w.children()[1]
    global filter
    if filter:
        viewportWidget.removeEventFilter(filter)
    filter = RenderViewEventFilter(viewportWidget)
    viewportWidget.installEventFilter(filter)

def deregisterRenderViewMarquee():
    ptr = omu.MQtUtil.findControl(maya.cmds.renderWindowEditor("renderView", q=1, ctl=1))
    w = shiboken.wrapInstance(long(ptr), QtGui.QWidget)
    viewportWidget = w.children()[1]
    global filter
    if filter:
        viewportWidget.removeEventFilter(filter)