/*!
 *
﻿ * Copyright (C) 2019 Technical University of Liberec.  All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation. (http://www.gnu.org/licenses/gpl-3.0.en.html)
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * 
 * @file    element_values.hh
 * @brief   Class ElementValues calculates data related to transformation
 *          of reference cell to actual cell (Jacobian, inverse Jacobian,
 *          determinant, point coordinates etc.).
 * @author  Jan Stebel
 */

#ifndef ELEMENT_VALUES_HH_
#define ELEMENT_VALUES_HH_

// #include <string.h>                           // for memcpy
// #include <algorithm>                          // for swap
// #include <new>                                // for operator new[]
// #include <string>                             // for operator<<
#include <vector>                             // for vector
#include "mesh/ref_element.hh"                // for RefElement
#include "mesh/accessors.hh"                  // for ElementAccessor
#include "fem/update_flags.hh"                // for UpdateFlags
#include "fem/dh_cell_accessor.hh"            // for DHCellAccessor, DHCellSide

class Quadrature;




/**
 * @brief Structure for storing the precomputed element data.
 */
class RefElementData
{
public:
    
    /// Resize vectors to size @p np.
    RefElementData(unsigned int np);
    
    /// Barycentric coordinates of quadrature points.
    std::vector<arma::vec> bar_coords;
    
    /// Quadrature weights.
    std::vector<double> weights;
    
    /// Number of quadrature points.
    unsigned int n_points;
};




/**
 * @brief Class ElementData holds the arrays of data computed by
 * Mapping.
 */
template<unsigned int spacedim = 3>
class ElementData
{
public:
    
    /**
     * @brief Resize the data arrays.
     * @param size   Number of quadrature points.
     * @param flags  Update flags to be stores.
     */
    ElementData(unsigned int size, UpdateFlags flags, unsigned int dim);
    
    /// Print calculated data.
    void print();

    /// Dimension of space of reference cell.
    const unsigned int dim_;

    /**
     * @brief Transformed quadrature weights.
     *
     * Values at quadrature points of the product of the Jacobian
     * determinant of the mapping and the weight at the particular
     * quadrature point.
     */
    std::vector<double> JxW_values;

    /// Jacobians (spacedim x dim) of the mapping at the quadrature points.
    Armor::array jacobians;

    /// Determinants of Jacobians at quadrature points.
    std::vector<double> determinants;

    /// Inverse Jacobians (dim x spacedim) at the quadrature points.
    Armor::array inverse_jacobians;

    /// Coordinates (spacedim) of quadrature points in the actual cell coordinate system.
    Armor::array points;

    /// Normal vectors (spacedim) to the element at the quadrature points lying on a side.
    Armor::array normal_vectors;

    /// Flags that indicate which finite element quantities are to be computed.
    UpdateFlags update_flags;

    /// Iterator to last updated cell.
    DHCellAccessor cell;

    /// Iterator to last updated cell side.
    DHCellSide side;

};



/**
 * @brief Base class for ElementValues and ElemSideValues
 */
template<unsigned int spacedim>
class ElementValuesBase
{
public:

    /**
     * Correct deallocation of objects created by 'initialize' methods.
     */
    virtual ~ElementValuesBase() {}


    /**
     * @brief Allocates space for computed data.
     *
     * @param n_points Number of quadrature points.
     * @param flags    The update flags.
     */
    ElementValuesBase(unsigned int n_points,
                      UpdateFlags flags,
                      unsigned int dim)
    : dim_(dim), n_points_(n_points), data(n_points, update_each(flags), dim) {}
    
    /**
     * @brief Determine quantities to be recomputed on each cell.
     *
     * @param flags Flags that indicate what has to be recomputed.
     */
    UpdateFlags update_each(UpdateFlags flags);
    
    /**
     * @brief Return the relative volume change of the cell (Jacobian determinant).
     *
     * If dim==spacedim then the sign may be negative, otherwise the
     * result is a positive number.
     *
     * @param point_no Number of the quadrature point.
     */
    inline double determinant(const unsigned int point_no) const
    {
        ASSERT_LT_DBG(point_no, n_points_);
        return data.determinants[point_no];
    }
    
    /// Return Jacobian matrix at point @p point_no.
    inline arma::mat jacobian(const unsigned int point_no) const
    {
        ASSERT_LT_DBG(point_no, n_points_);
        return data.jacobians.arma_mat(point_no);
    }
    
    /// Return inverse Jacobian matrix at point @p point_no.
    inline arma::mat inverse_jacobian(const unsigned int point_no) const
    {
        ASSERT_LT_DBG(point_no, n_points_);
        return data.inverse_jacobians.arma_mat(point_no);
    }

    /**
     * @brief Return the product of Jacobian determinant and the quadrature
     * weight at given quadrature point.
     *
     * @param point_no Number of the quadrature point.
     */
    inline double JxW(const unsigned int point_no) const
    {
        ASSERT_LT_DBG(point_no, n_points_);
        return data.JxW_values[point_no];
    }

    /**
     * @brief Return coordinates of the quadrature point in the actual cell system.
     *
     * @param point_no Number of the quadrature point.
     */
    inline Armor::vec<spacedim> point(const unsigned int point_no) const
    {
        ASSERT_LT_DBG(point_no, n_points_);
        return data.points.get<spacedim>(point_no);
    }

    /// Return coordinates of all quadrature points in the actual cell system.
	inline const Armor::array &point_list() const
	{
	    return data.points;
	}


    /**
     * @brief Returns the normal vector to a side at given quadrature point.
     *
     * @param point_no Number of the quadrature point.
     */
	inline Armor::vec<spacedim> normal_vector(unsigned int point_no)
	{
        ASSERT_LT_DBG(point_no, n_points_);
	    return data.normal_vectors.get<spacedim>(point_no);
	}
	
    /// Returns the number of quadrature points.
    inline unsigned int n_points()
    { return n_points_; }
    
    /// Return cell at which the values were reinited.
    const DHCellAccessor &cell() const
    { return data.cell; }

    /// Return cell side where the values were reinited.
    const DHCellSide &side() const
    { return data.side; }



protected:
    
    /// Precompute data on reference element.
    RefElementData *init_ref_data(const Quadrature &q);
    
    /// Dimension of space of reference cell.
    unsigned int dim_;

    /// Number of integration points.
    unsigned int n_points_;

    /// Data computed by the mapping.
    ElementData<spacedim> data;
    
};




/**
 * @brief Calculates element data on the actual cell.
 *
 * @param spacedim Dimension of the Euclidean space where the actual
 *                 cell lives.
 */
template<unsigned int spacedim = 3>
class ElementValues : public ElementValuesBase<spacedim>
{
public:

	/**
	 * @brief Constructor.
	 *
	 * Initializes structures and calculates
     * cell-independent data.
	 *
	 * @param _quadrature The quadrature rule.
	 * @param _flags      The update flags.
     * @param dim         Dimension of space of reference cell.
	 */
    ElementValues(Quadrature &_quadrature,
             UpdateFlags _flags,
             unsigned int dim);
    
    ~ElementValues();

    /**
     * @brief Update cell-dependent data (gradients, Jacobians etc.)
     *
     * @param cell The actual cell.
     */
    void reinit(const DHCellAccessor &cell);
    
    
private:
    
    /// Compute data from reference cell and using MappingP1.
    template<unsigned int dim>
    void fill_data();
    
    /// Precomputed element data.
    RefElementData *ref_data;


};




/**
 * @brief Calculates element data on a side.
 *
 * @param spacedim Dimension of the Euclidean space where the actual
 *                 cell lives.
 */
template<unsigned int spacedim = 3>
class ElemSideValues : public ElementValuesBase<spacedim>
{

public:

    /**
     * @brief Constructor.
     *
     * Initializes structures and calculates
     * cell-independent data.
     *
     * @param _sub_quadrature The quadrature rule on the side (with dimension dim-1).
     * @param flags           The update flags.
     * @param dim             Dimension of space of reference cell.
     */
    ElemSideValues(Quadrature &_sub_quadrature,
             UpdateFlags flags,
             unsigned int dim);

    /// Destructor.
    virtual ~ElemSideValues();

    /**
	 * @brief Update cell-dependent data (Jacobians etc.)
	 *
	 * @param cell_side The actual cell and side.
	 */
    void reinit(const DHCellSide &cell_side);



private:
    
    /**
     * @brief Calculates the mapping data on a side of a cell.
     */
    template<unsigned int dim>
    void fill_side_data();

    /// Number of sides in reference cell.
    const unsigned int n_sides_;

    /// Number of permutations of points on side of reference cell.
    const unsigned int n_side_permutations_;

    /// Data on reference element (for each side and its permutation).
    std::vector<std::vector<RefElementData*>> side_ref_data;
    
};





#endif /* ELEMENT_VALUES_HH_ */