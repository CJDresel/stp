// -*- c++ -*-
/********************************************************************
 * AUTHORS: Vijay Ganesh
 *
 * BEGIN DATE: November, 2005
 *
 * LICENSE: Please view LICENSE file in the home dir of this Program
 ********************************************************************/
#include "ToSAT.h"

namespace BEEV
{

  bool isTseitinVariable(const ASTNode& n) {
    if (n.GetKind() == SYMBOL && n.GetType() == BOOLEAN_TYPE) {
      const char * zz = n.GetName();
      //if the variables ARE cnf variables then dont make them
      // decision variables.
      if (0 == strncmp("cnf", zz, 3))
        {
          return true;
        }
    }
    return false;
  }

  /* FUNCTION: lookup or create a new MINISAT literal
   * lookup or create new MINISAT Vars from the global MAP
   * _ASTNode_to_SATVar.
   */

  MINISAT::Var 
  ToSAT::LookupOrCreateSATVar(MINISAT::Solver& newSolver, const ASTNode& n)
  {
    ASTtoSATMap::iterator it;
    MINISAT::Var v;

    //look for the symbol in the global map from ASTNodes to ints. if
    //not found, create a S.newVar(), else use the existing one.
    if ((it = _ASTNode_to_SATVar_Map.find(n)) == _ASTNode_to_SATVar_Map.end())
      {
        v = newSolver.newVar();
        _ASTNode_to_SATVar_Map[n] = v;

        //ASSUMPTION: I am assuming that the newSolver.newVar() call increments v
        //by 1 each time it is called, and the initial value of a
        //MINISAT::Var is 0.
        _SATVar_to_AST_Vector.push_back(n);

        // experimental. Don't add Tseitin variables as decision variables.
        if (!bm->UserFlags.tseitin_are_decision_variables_flag && isTseitinVariable(n))
          {
            newSolver.setDecisionVar(v,false);
          }

      }
    else
      v = it->second;
    return v;
  }

  /* FUNCTION: convert ASTClauses to MINISAT clauses and solve.
   * Accepts ASTClauses and converts them to MINISAT clauses. Then
   * adds the newly minted MINISAT clauses to the local SAT instance,
   * and calls solve(). If solve returns unsat, then stop and return
   * unsat. else continue.
   */
  bool ToSAT::toSATandSolve(MINISAT::Solver& newSolver,
                            ClauseList& cll, 
			    bool add_xor_clauses,
			    bool enable_clausal_abstraction)
  {
    CountersAndStats("SAT Solver", bm);
    bm->GetRunTimes()->start(RunTimes::SendingToSAT);

    
    int input_clauselist_size = cll.size();
    if (cll.size() == 0)
      {
        FatalError("toSATandSolve: Nothing to Solve", ASTUndefined);    
      }

    if(bm->UserFlags.random_seed_flag)
      {
	newSolver.setSeed(bm->UserFlags.random_seed);
	//cerr << "We have set the seed value to "
	//   << bm->UserFlags.random_seed 
	//   << endl;
      }

    if(bm->UserFlags.print_cnf_flag)
      {
	//newSolver.cnfDump = true;
      }
#ifdef CRYPTOMINISAT
    newSolver.startClauseAdding();
#endif
    //iterate through the list (conjunction) of ASTclauses cll
    ClauseList::const_iterator i = cll.begin(), iend = cll.end();    
    for (int count=0, flag=0; i != iend; i++)
      {
        //Clause for the SATSolver
        MINISAT::vec<MINISAT::Lit> satSolverClause;
        satSolverClause.capacity((*i)->size());        
        vector<const ASTNode*>::const_iterator j    = (*i)->begin(); 
        vector<const ASTNode*>::const_iterator jend = (*i)->end();      
        //ASTVec  clauseVec;
        //j is a disjunct in the ASTclause (*i)
        for (; j != jend; j++)
          {         
            ASTNode node = **j;
            //clauseVec.push_back(node);
            bool negate = (NOT == node.GetKind()) ? true : false;
            ASTNode n = negate ? node[0] : node;
            MINISAT::Var v = LookupOrCreateSATVar(newSolver, n);
            MINISAT::Lit l(v, negate);
            satSolverClause.push(l);
          }

        // ASTNode theClause = bm->CreateNode(OR, clauseVec);
        //      if(flag 
        //         && ASTTrue == CheckBBandCNF(newSolver, theClause))
        //        {
        //          continue;
        //        }
#if defined CRYPTOMINISAT || defined CRYPTOMINISAT2
        if(add_xor_clauses)
          {         
            //cout << "addXorClause:\n";
            newSolver.addXorClause(satSolverClause, false, 0, "z");
          }
        else 
          {
            newSolver.addClause(satSolverClause,0,"z");
          }
#else
        newSolver.addClause(satSolverClause);
#endif
        if(enable_clausal_abstraction && 
	   count++ >= input_clauselist_size*CLAUSAL_ABSTRACTION_CUTOFF)
          {
            //Arbitrary adding only 60% of the clauses in the hopes of
            //terminating early 
            //      cout << "Percentage clauses added: " 
            //           << percentage << endl;
            bm->GetRunTimes()->stop(RunTimes::SendingToSAT);
            bm->GetRunTimes()->start(RunTimes::Solving);

#if defined CRYPTOMINISAT2
	    newSolver.set_gaussian_decision_until(100);
	    newSolver.performReplace = false;
	    newSolver.xorFinder = false;
#endif
	    newSolver.solve();
            bm->GetRunTimes()->stop(RunTimes::Solving);
            if(!newSolver.okay())
              {
                return false;         
              }
            count = 0;
            flag  = 1;
            bm->GetRunTimes()->start(RunTimes::SendingToSAT);
          }
        if (newSolver.okay())
          {
            continue;
          }     
        else
          {
            bm->PrintStats(newSolver);
            bm->GetRunTimes()->stop(RunTimes::SendingToSAT);
            return false;
          }     
      } // End of For-loop adding the clauses 

    bm->GetRunTimes()->stop(RunTimes::SendingToSAT);
    bm->GetRunTimes()->start(RunTimes::Solving);
    
    newSolver.solve();
    bm->GetRunTimes()->stop(RunTimes::Solving);
    bm->PrintStats(newSolver);
    if (newSolver.okay())
      return true;
    else
      return false;
  } //end of toSATandSolve()

#if 0

  // Looks up truth value of ASTNode SYMBOL in MINISAT satisfying
  // assignment.
  ASTNode ToSAT::SymbolTruthValue(MINISAT::Solver &newSolver, ASTNode form)
  {
    MINISAT::Var satvar = _ASTNode_to_SATVar_Map[form];
    if (newSolver.model[satvar] == MINISAT::l_False)
      {
        return ASTFalse;
      }
    else
      {
        // True or undefined.
        return ASTTrue;
      }
  }

  // This function is for debugging problems with BitBlast and
  // especially ToCNF. It evaluates the bit-blasted formula in the
  // satisfying assignment.  While doing that, it checks that every
  // subformula has the same truth value as its representative
  // literal, if it has one.  If this condition is violated, it halts
  // immediately (on the leftmost lowest term).  Use CreateSimpForm to
  // evaluate, even though it's expensive, so that we can use the
  // partial truth assignment.
  ASTNode ToSAT::CheckBBandCNF(MINISAT::Solver& newSolver, ASTNode form)
  {
    // Clear memo table (in case newSolver has changed).
    CheckBBandCNFMemo.clear();
    // Call recursive version that does the work.
    return CheckBBandCNF_int(newSolver, form);
  } //End of CheckBBandCNF()

  // Recursive body CheckBBandCNF
  ASTNode ToSAT::CheckBBandCNF_int(MINISAT::Solver& newSolver, ASTNode form)
  {
    //     cout << "++++++++++++++++" 
    //   << endl 
    //   << "CheckBBandCNF_int form = " 
    //   << form << endl;
    
    ASTNodeMap::iterator memoit = CheckBBandCNFMemo.find(form);
    if (memoit != CheckBBandCNFMemo.end())
      {
        // found it.  Return memoized value.
        return memoit->second;
      }

    ASTNode result; // return value, to memoize.

    Kind k = form.GetKind();
    switch (k)
      {
      case TRUE:
      case FALSE:
        {
          return form;
          break;
        }
      case SYMBOL:
      case BVGETBIT:
        {
          result = SymbolTruthValue(newSolver, form);

          //           cout << "================" 
          //                << endl 
          //                << "Checking BB formula:" 
          //                << form << endl;
          //           cout << "----------------" 
          //                << endl 
          //                << "Result:" << result << endl;
          break;
        }
      default:
        {
          // Evaluate the children recursively.
          ASTVec eval_children;
          ASTVec ch = form.GetChildren();
          ASTVec::iterator itend = ch.end();
          for (ASTVec::iterator it = ch.begin(); it < itend; it++)
            {
              eval_children.push_back(CheckBBandCNF_int(newSolver, *it));
            }
          result = bm->CreateSimpForm(k, eval_children);

          //           cout << "================" 
          //                << endl 
          //                << "Checking BB formula:" << form << endl;
          //           cout << "----------------" 
          //                << endl 
          //                << "Result:" << result << endl;
          
          ASTNode replit_eval;
          // Compare with replit, if there is one.
          ASTNodeMap::iterator replit_it = RepLitMap.find(form);
          if (replit_it != RepLitMap.end())
            {
              ASTNode replit = RepLitMap[form];
              // Replit is symbol or not symbol.
              if (SYMBOL == replit.GetKind())
                {
                  replit_eval = SymbolTruthValue(newSolver, replit);
                }
              else
                {
                  // It's (NOT sym).  Get value of sym and complement.
                  replit_eval = 
                    bm->CreateSimpNot(SymbolTruthValue(newSolver, replit[0]));
                }

              //               cout << "----------------" 
              //                    << endl 
              //                    << "Rep lit: " << replit << endl;
              //               cout << "----------------" 
              //                    << endl 
              //                    << "Rep lit value: " << replit_eval << endl;

              if (result != replit_eval)
                {
                  // Hit the panic button.
                  FatalError("Truth value of BitBlasted formula "\
                             "disagrees with representative literal in CNF.");
                }
            }
          else
            {
              //cout << "----------------" << endl << "No rep lit" << endl;
            }

        }
      }

    return (CheckBBandCNFMemo[form] = result);
  } //end of CheckBBandCNF_int()
#endif
}; //end of namespace BEEV
