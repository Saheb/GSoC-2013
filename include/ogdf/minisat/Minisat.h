/*
 * $Revision: 2523 $
 *
 * last checkin:
 *   $Author: gutwenger $
 *   $Date: 2012-07-02 20:59:27 +0200 (Mo, 02. Jul 2012) $
 ***************************************************************/

/** \file
 * \brief Declaration of class Minisat.
 *
 * \author Eldor Malessa, Robert Zeranski
 *
 * \par License:
 * This file is part of the Open Graph Drawing Framework (OGDF).
 *
 * \par
 * Copyright (C)<br>
 * See README.txt in the root directory of the OGDF installation for details.
 *
 * \par
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * Version 2 or 3 as published by the Free Software Foundation;
 * see the file LICENSE.txt included in the packaging of this file
 * for details.
 *
 * \par
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * \see  http://www.gnu.org/copyleft/gpl.html
 ***************************************************************/


#ifndef __minisat_h_
#define __minisat_h_

#include <ogdf/basic/basic.h>

#include <ogdf/minisat/core/Solver.h>
#include <ogdf/minisat/core/SolverTypes.h>

#include <stdio.h>
#include <iostream>
#include <fstream>

#include <stdarg.h>

#include <string>
#include <sstream>

#include <vector>

namespace Minisat
{

/**
* Represents a simple class for clause storage.
* This class is not similar to clause from Minisat.
* Only use it as a wrapper.
* Use clause as a pointer-type of Clause.
*/
typedef class Clause
{
	public:

		MinisatInternal::vec<MinisatInternal::Lit> m_ps;
		Clause(){}
		Clause( const Clause &src ) { src.m_ps.copyTo(m_ps); }
		virtual ~Clause(){}

		// this function allows to put signed Vars directly
		// because Vars in Solver are starting with 0 (not signable with "-")
		// values in input are staring with 1/-1 and are recalculated to 0-base in this function
		//! adds a literal to the clause
		/**
		* @param signedVar is a signed int representing a variable
		*/
		void add ( MinisatInternal::Var signedVar )
		{
			MinisatInternal::Lit l;
			if ( signedVar >= 0 )
			{
				l = MinisatInternal::mkLit(signedVar-1, true);
			}
			else
			{
				l = MinisatInternal::mkLit( (signedVar+1) * (-1), false);
			}
			m_ps.push(l);
		}

		//! add multiple literals to the clause
		/**
		* @param Amount is the number of literals to add to the clause
		*/
		void addMultiple ( int Amount, ... );

		//! sets the sign of a variable if its present within the clause
		void setSign ( MinisatInternal::Var x, bool sign )
		{
			for ( int i = 0; i < m_ps.size(); i++ )
			{
				if ( MinisatInternal::var (m_ps[i] ) == x )
				{
					m_ps[i] = MinisatInternal::mkLit( x, sign );
					break;
				}
			}
		}

		//! returns the sign of a variable if its present within the clause,
		//! if the variable is not representet false will be returned with a message
		bool getSign ( MinisatInternal::Var x )
		{
			for ( int i = 0; i < m_ps.size(); i++ )
			{
				if ( MinisatInternal::var (m_ps[i] ) == x )

				{
					return MinisatInternal::sign(m_ps[i]);
					break;
				}
			}
			std::cout << "Variable not in Clause" << std::endl;
			return false;
		}

		void removeLit( MinisatInternal::Var x )
		{
			//the vec-class doesn't allow to erase elements at a position
			MinisatInternal::vec<MinisatInternal::Lit> help; m_ps.copyTo(help);
			m_ps.clear();
			for ( int i = 0; i < m_ps.size(); i++ )
			{
				if ( MinisatInternal::var (m_ps[i] ) != x )
				{
					m_ps.push( help[i] );
				}

			}
		}

		//! converts the sign of a lit into char
		static char convertLitSign( MinisatInternal::Lit l )
		{
			if ( sign(l) == 0 )
				return '-';
			else
				return ' ';
		}

		void writeToConsole()
		{
			for ( int i = 0; i < m_ps.size() - 1; i++ )
				std::cout << convertLitSign( m_ps[i] ) << MinisatInternal::var( m_ps[i] ) + 1 << " || ";
			std::cout << convertLitSign( m_ps[ m_ps.size() - 1 ] ) << MinisatInternal::var( m_ps[ m_ps.size() - 1 ] ) + 1 << std::endl;
		}
} *clause;

/**
* Represents a simple class for model storage.
* A model is a feasible assignment of variables.
*/
class Model
{
	private:

		//! internal storage of a model by minisat
		std::vector<int> m_vModel;

		void reset()
		{
			m_vModel.clear();
		}

	public :

		Model() {};
		virtual ~Model() { reset(); }

		//! returns the value of the assignemt of a variable in the model
		bool getValue (int var) const
		{
			OGDF_ASSERT(var > 0 && var <= (int)m_vModel.size());
			return m_vModel[var-1] != 0;
		}

		//! sets the model to the model of minsat
		void setModel ( MinisatInternal::Solver &S )
		{
			reset();

			m_vModel.reserve( S.model.size() );
			for ( int i = 0; i < S.model.size() ; i++ )
			{
				m_vModel.push_back( MinisatInternal::toInt( S.model[i] ) );
			}
		}

		void printModel()
		{
			for ( int i = 0; i < (int)m_vModel.size(); i++ )
			{
				std::cout << "Var " << i << " = " << m_vModel[i] << " ";
			}
			std::cout << std::endl;
		}

		std::string intToString ( const int i )
		{
			std::string s;
			switch ( i )
			{
				case 0:
					s = "False";
					break;
				case 1:
					s = "True";
					break;
				case 2:
					s = "Undefined";
					break;
				default :
					s = "";
			};
			return s;
		};
};

/**
* The Formula class.
* Variables and Clauses are added to the Formula.
* The clauses can be resolved to solve a SAT problem.
* Variables are linear indexed.
*/
class Formula : private MinisatInternal::Solver
{
	private:

		std::stringstream m_messages;
		std::vector<Clause*> m_Clauses;

		void free();

	public:
		Formula() {}
		virtual ~Formula(){ free(); }

	//! add a new variable to the formula
	void newVar ()
	{
		Solver::newVar();
	}

	//! add multiple new variables to the formula
	void newVars ( unsigned int Count )
	{
		//proofs if variables from clause are valid (still known to formula)
		for ( unsigned int i = 0; i < Count; i++ )
			Solver::newVar();
	}

	//! creates a new clause within the formula. If not all variables are known, missing ones are generated auomatically
	/**
	* \returns a Pointer to the created Clause object
	*/
	Clause *newClause();

	//! adds a clause to the formula's solver. If not all variables are known, missing ones are generated auomatically
	/**
	* @param cl is a reference to the clause to be added to the formula
	*/
	void finalizeClause( const clause cl );

	//! adds a clause to the formula's solver if all variables are known
	/**
	* @param cl is a reference to an existing clause within the formula
	* \returns true if the clause was successfully added to the formula's solver, else return false and the clause is NOT added to the formula's solver
	*/
	bool finalizeNotExtensibleClause ( const clause cl );

	//! returns a clause at position pos in Formula
	Clause *getClause ( const int pos );

	//! removes a complete clause
	void removeClause( int i );

	//! count of problem clauses
	int getProblemClauseCount()
	{
		return nClauses();
	}

	//! count of learned clauses
	int getClauseCount()
	{
		return (int)m_Clauses.size();
	}

	//! returns varable count currently in solver
	int getVariableCount()
	{
		return nVars();
	}

	//! tries to solve the formula
	/**
	* @param ReturnModel is the model output
	* \returns true if the problem is satisfiable and writes the output model to param
	*/
	bool solve( Model &ReturnModel );

	MinisatInternal::Var getVarFromLit( const MinisatInternal::Lit l )
	{
		return MinisatInternal::var(l);
	}

	//! adds a literal to a clause and
	//! moves clause to the end of list
	void clauseAddLiteral( unsigned int clausePos, MinisatInternal::Var signedVar )
	{
		removeClause( clausePos );
		m_Clauses[clausePos]->add(signedVar);
		Solver::addClause( m_Clauses[clausePos]->m_ps );
	}

	//! delete all clauses and variables
	void reset();

	//! read a formula from a DIMACS-file
	bool readDimacsFile( const char *file );

	//! write a formula to a DIMACS-file
	void writeFormulaToDimacs( const char *filename);
};

}
#endif // __minisat_h_
