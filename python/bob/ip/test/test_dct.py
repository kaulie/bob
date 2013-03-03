#!/usr/bin/env python
# vim: set fileencoding=utf-8 :
# Laurent El Shafey <Laurent.El-Shafey@idiap.ch>
# Sun Mar 3 18:53:00 2013 +0200
#
# Copyright (C) 2011-2013 Idiap Research Institute, Martigny, Switzerland
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""Test the DCTFeatures extractor
"""

import os, sys
import unittest
import bob
import numpy

A_org    = numpy.array(range(1,17), 'float64').reshape((4,4))
A_ans_0_3D  = numpy.array([[[1, 2], [5, 6]], [[3, 4], [7, 8]], [[9, 10], [13, 14]], [[11, 12], [15, 16]]], 'float64')
A_ans_0_4D  = numpy.array([[[[1, 2], [5, 6]], [[3, 4], [7, 8]]], [[[9, 10], [13, 14]], [[11, 12], [15, 16]]]], 'float64')

src = numpy.array([[0, 1, 2, 3, 4, 5, 6, 7],
  [ 8,  9, 10, 11, 12, 13, 14, 15],
  [16, 17, 18, 19, 20, 21, 22, 23], 
  [24, 25, 26, 27, 28, 29, 30, 31], 
  [32, 33, 34, 35, 36, 37, 38, 39], 
  [40, 41, 42, 43, 44, 45, 46, 47]], numpy.float64)

dst1 = numpy.array([32.9090, -3.8632, -22.6274, 0., 0., 0.], numpy.float64);
dst2 = numpy.array([46.7654, -3.8632, -22.6274, 0., 0., 0.], numpy.float64);
dst3 = numpy.array([116.0474, -3.8632, -22.6274, 0., 0., 0.], numpy.float64);
dst4 = numpy.array([129.9038, -3.8632, -22.6274, 0., 0., 0.], numpy.float64);
dst_mat = [dst1, dst2, dst3, dst4]

# Reference values from the former (Idiap internal) facereclib python scripts
srcB = numpy.array([[1.,3.,5.,2.], [5.,7.,3.,2.], [4.,7.,6.,1.], [1.,3.,5.,4.]], numpy.float64);
dstB_ff = numpy.array([[8., -2., -4.], [6., 2., 1.], [7.5, -2.5, 3.5], [8., 3., -1.]], numpy.float64);
dstB_tf = numpy.array([[0., -0.89442719, -1.78885438], [0., 1.63299316, 0.81649658],
            [0., -1.15470054, 1.61658075], [0., 1.60356745, -0.53452248]], numpy.float64);
dstB_ft = numpy.array([[0.76249285, -0.88259602, -1.41054884], [-1.67748427, 0.7787612, 0.40951418],
            [0.15249857, -1.09026568, 1.31954569], [0.76249285, 1.1941005, -0.31851103]], numpy.float64);
dstB_tt = numpy.array([[0., -0.89931199, -1.39685855], [0., 1.00866019, 0.60685661],
            [0., -1.09579466, 1.22218284], [0., 0.98644646, -0.4321809]], numpy.float64);


class DCTFeaturesTest(unittest.TestCase):
  """Performs various tests for the zigzag extractor."""

  def test01_extract_arrays(self):
    dct_op = bob.ip.DCTFeatures( 3, 4, 0, 0, 6)
    dst = dct_op(src)
    self.assertTrue( len(dst) == len(dst_mat) )
    for i in range(len(dst)):
      self.assertTrue( numpy.allclose(dst[i], dst_mat[i], 1e-5, 1e-4) )

    dst = dct_op(src, True)
    self.assertTrue( dst.shape[0] == len(dst_mat) )
    for i in range(len(dst_mat)):
      self.assertTrue( numpy.allclose(dst[i,:], dst_mat[i], 1e-5, 1e-4) )

  def test02_extract_arrays_normalize(self):
    dct_op = bob.ip.DCTFeatures( 2, 2, 0, 0, 3)
    dst = dct_op(srcB, True);
    self.assertTrue( numpy.allclose(dst, dstB_ff, 1e-5, 1e-8) )

    dct_op.norm_block = True
    dst = dct_op(srcB, True);
    self.assertTrue( numpy.allclose(dst, dstB_tf, 1e-5, 1e-8) )
    
    dct_op.norm_block = False
    dct_op.norm_dct = True
    dst = dct_op(srcB, True);
    self.assertTrue( numpy.allclose(dst, dstB_ft, 1e-5, 1e-8) )

    dct_op.norm_block = True
    dst = dct_op(srcB, True);
    self.assertTrue( numpy.allclose(dst, dstB_tt, 1e-5, 1e-8) )
