#ifndef LINEAR_REACTION_H
#define LINEAR_REACTION_H

#include <vector>

#include "reaction/linear_reaction_base.hh"
#include "input/accessors.hh"

class Mesh;
class Distribution;

/** @brief Class implements the linear reactions.
 * 
 * This class implements the user interface for linear reactions and prepares the reaction matrix.
 * Common features are inherited from the @p FirstOrderReactionBase class.
 */
class FirstOrderReaction: public FirstOrderReactionBase
{
public:
    /**
     * Static variable for new input data types input
     */
    static Input::Type::Record input_type;
    /**
     * Static variable gets information about particular decay step
     */
    static Input::Type::Record input_type_single_reaction;

    /// Constructor.
    FirstOrderReaction(Mesh &init_mesh, Input::Record in_rec);

    /// Destructor.
    ~FirstOrderReaction(void);
    
protected:

    /// Implements the assembly of the system matrix of the ODEs.
    void assemble_ode_matrix(void) override;
    
    /// Initializes private members of sorption from the input record.
    void initialize_from_input();
    
    std::vector<double> reaction_rates_;    ///< Vector of reaction rates of the transported substances.
};

#endif  // LINEAR_REACTION_H
