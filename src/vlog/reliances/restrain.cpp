#include "vlog/reliances/reliances.h"

#include <vector>
#include <utility>
#include <list>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

struct RestrainPiece
{
    std::vector<unsigned> LiteralIndices;
};

struct MarkResult
{
    MarkResult(uint32_t ruleId, const std::vector<Literal> &body, const std::vector<Literal> &heads, const std::vector<std::vector<Literal>> &headSplit) 
        : singleRule(ruleId, heads, body), headPieces(headSplit) {}

    Rule singleRule;
    std::vector<std::vector<Literal>> headPieces;
};

void createPiece(const std::vector<Literal> &heads, unsigned currentLiteralIndex, std::vector<bool> &literalsInPieces, std::vector<Literal> &result)
{
    bool validPiece = false; //Piece has to contain an existential variable

    const Literal &currentLiteral = heads[currentLiteralIndex];
    literalsInPieces[currentLiteralIndex] = true;

    for (unsigned termIndex = 0; termIndex < currentLiteral.getTupleSize(); ++termIndex)
    {
        VTerm currentTerm = currentLiteral.getTermAtPos(termIndex);

        if ((int)currentTerm.getId() >= 0)
            continue;

        validPiece = true;

        for (unsigned searchIndex = currentLiteralIndex + 1; searchIndex < heads.size(); ++searchIndex)
        {
            if (literalsInPieces[searchIndex])
                continue;

            const Literal &currentSearchedLiteral = heads[searchIndex];
            for (unsigned searchTermIndex = 0; searchTermIndex < currentSearchedLiteral.getTupleSize(); ++searchTermIndex)
            {
                VTerm currentSearchedTerm = currentSearchedLiteral.getTermAtPos(searchTermIndex);
            
                if (currentTerm.getId() == currentSearchedTerm.getId())
                {
                    createPiece(heads, searchIndex, literalsInPieces, result);
                    break;
                }
            }
        }
    }

    if (validPiece)
        result.push_back(currentLiteral);
}

MarkResult markExistentialVariablesAndPieces(const Rule &rule)
{
    uint32_t ruleId = rule.getId();
    std::vector<Literal> body = rule.getBody();
    std::vector<Literal> heads;

    std::vector<Var_t> existentialVariables = rule.getExistentialVariables();

    for (const Literal &literal: rule.getHeads())
    {
        VTuple *tuple = new VTuple(literal.getTupleSize());

        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);;

            if (currentTerm.getId() > 0 && std::find(existentialVariables.begin(), existentialVariables.end(), currentTerm.getId()) != existentialVariables.end())
            {
                currentTerm.setId(-currentTerm.getId());
            }

            tuple->set(currentTerm, termIndex);
        }

        heads.push_back(Literal(literal.getPredicate(), *tuple, literal.isNegated()));
    }

    std::vector<std::vector<Literal>> headsSplit;
    std::vector<bool> literalsInPieces;
    literalsInPieces.resize(heads.size(), false);

    for (unsigned literalIndex = 0; literalIndex < heads.size(); ++literalIndex)
    {
        const Literal &currentLiteral = heads[literalIndex];
        
        if (literalsInPieces[literalIndex])
            continue;

        std::vector<Literal> newPiece;
        createPiece(heads, literalIndex, literalsInPieces, newPiece);
        if (newPiece.size() > 0)
            headsSplit.push_back(newPiece);
    }

    return MarkResult(ruleId, body, heads, headsSplit);
}

int64_t restrainGetAssignedConstant(VTerm term,
    const std::vector<int64_t> &assignment, const std::vector<VariableAssignments::Group> &groups)
{
    if (term.getId() == 0) // constant
        return term.getValue();   
    else // variable 
    {
        if (assignment[std::abs((int32_t)term.getId())] == NOT_ASSIGNED)
            return NOT_ASSIGNED;
        else
            return groups[assignment[std::abs((int32_t)term.getId())]].value;
    }
}

bool restrainExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo, const std::vector<Literal> &ruleToPiece,
    const VariableAssignments &assignments);

bool restrainModels(const std::vector<Literal> &left, const std::vector<int64_t> &leftAssignment,
    const std::vector<Literal> &right, const std::vector<int64_t> &rightAssignment,
    const std::vector<VariableAssignments::Group> &groups, 
    std::vector<unsigned> &satisfied, bool sameRule)
{
    bool isCompletelySatisfied = true;

    for (unsigned rightIndex = 0; rightIndex < right.size(); ++rightIndex)
    {   
        if (satisfied[rightIndex] == 1)
            continue;

        const Literal &rightLiteral = right[rightIndex];

        bool rightSatisfied = false;
        for (const Literal &leftLiteral : left)
        {
            if (rightLiteral.getPredicate().getId() != leftLiteral.getPredicate().getId())
                continue;

            bool leftModelsRight = true;
            unsigned tupleSize = leftLiteral.getTupleSize(); //Should be the same as rightLiteral.getTupleSize()

            for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
            {
                VTerm leftTerm = leftLiteral.getTermAtPos(termIndex);
                VTerm rightTerm = rightLiteral.getTermAtPos(termIndex);
           
                int64_t leftConstant = restrainGetAssignedConstant(leftTerm, leftAssignment, groups);
                int64_t rightConstant = positiveGetAssignedConstant(rightTerm, rightAssignment, groups);
            
                // Since universal variables cannot be assigned to nulls and we use positiveGetAssignedConstant
                // to get rightConstant, a negative value must mean that the term is an existential variable
                // which we treat as a placeholder which matches anything (if it is on the right side)
                if (rightConstant < 0)
                    continue;

                if (leftConstant != rightConstant)
                {
                    leftModelsRight = false;
                    break;
                }
                
                if (leftConstant == NOT_ASSIGNED) //it follows that rightConstant == NOT_ASSIGNED
                {
                    int64_t leftGroup = leftAssignment[std::abs((int32_t)(leftTerm.getId()))];
                    int64_t rightGroup = rightAssignment[std::abs((int32_t)(rightTerm.getId()))];
                    
                    if (leftGroup == rightGroup)
                    {
                        if (leftGroup == NOT_ASSIGNED) //and rightGroup == NOT_ASSIGNED
                        {
                            if (sameRule && leftTerm.getId() == rightTerm.getId())
                            {
                                continue;
                            }
                            else
                            {
                                leftModelsRight = false;
                                break;
                            }
                        }
                        else
                        {
                            continue;
                        }
                    }
                    else
                    {
                        leftModelsRight = false;
                        break;
                    }
                }
                else //both are 'real' constants with the same value
                {
                    continue;
                }
            }  

            if (leftModelsRight)
            {
                rightSatisfied = true;
                break;
            }
        }
    
        if (rightSatisfied)
        {
            satisfied[rightIndex] = 1;
        }
        else
        {
            isCompletelySatisfied = false;
        }
    }

    return isCompletelySatisfied;
}

bool checkUnmappedExistentialVariables(const std::vector<Literal> &literals,
    const std::vector<int64_t> &assignment, const std::vector<VariableAssignments::Group> &groups)
{
    for (const Literal &literal : literals)
    {
        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);
            int64_t groupId = assignment[std::abs((int32_t)currentTerm.getId())];

            if ((int32_t)currentTerm.getId() < 0 && groupId != NOT_ASSIGNED)
            {
                const VariableAssignments::Group &group = groups[groupId];
                
                if (group.value < 0) // value is assigned to a null
                    return false;
            }                
        }
    }

    return true;
}

bool positiveModels(const std::vector<Literal> &left, const std::vector<int64_t> &leftAssignment,
    const std::vector<Literal> &right, const std::vector<int64_t> &rightAssignment,
    const std::vector<VariableAssignments::Group> &groups, 
    std::vector<unsigned> &satisfied, bool sameRule, bool treatExistentialAsConstant);

bool restrainCheck(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo, const std::vector<Literal> &ruleToPiece,
    const VariableAssignments &assignments)
{
    unsigned nextInDomainIndex = 0;
    std::vector<Literal> notMappedPieceLiterals;
    notMappedPieceLiterals.reserve(ruleToPiece.size());
    for (unsigned headIndex = 0; headIndex < ruleToPiece.size(); ++headIndex)
    {
        if (headIndex == mappingDomain[nextInDomainIndex])
        {
            if (nextInDomainIndex < mappingDomain.size() - 1)
            {
                ++nextInDomainIndex;
            }

            continue;
        }

        notMappedPieceLiterals.push_back(ruleToPiece[headIndex]);
    }

    // if (!checkUnmappedExistentialVariables(notMappedPieceLiterals, assignments.to, assignments.groups))
    // {
    //     return restrainExtend(mappingDomain, ruleFrom, ruleTo, ruleToPiece, assignments);
    // }

    std::vector<unsigned> satisfied;
    satisfied.resize(ruleTo.getHeads().size());

    bool toHeadSatisfied = false;
    // toHeadSatisfied |= positiveModels(ruleTo.getBody(), assignments.to, ruleTo.getHeads(), assignments.to, assignments.groups, satisfied, true);
    
    if (toHeadSatisfied)
        return false;

    satisfied.clear();
    satisfied.resize(ruleToPiece.size(), 0);

    bool pieceSatisfied = false;
    // pieceSatisfied |= restrainModels(ruleTo.getBody(), assignments.to, ruleToPiece, assignments.to, assignments.groups, satisfied, true);
    // pieceSatisfied |= restrainModels(ruleTo.getHeads(), assignments.to, ruleToPiece, assignments.to, assignments.groups, satisfied, true);
    // pieceSatisfied |= restrainModels(ruleFrom.getBody(), assignments.from, ruleToPiece, assignments.to, assignments.groups, satisfied, false);
    // pieceSatisfied |= restrainModels(notMappedPieceLiterals, assignments.to, ruleToPiece, assignments.to, assignments.groups, satisfied, true);

    if (pieceSatisfied)
        return restrainExtend(mappingDomain, ruleFrom, ruleTo, ruleToPiece, assignments);

    satisfied.clear();
    satisfied.resize(ruleFrom.getHeads().size(), 0);

    bool fromHeadSatisfied = false;
    // fromHeadSatisfied |= positiveModels(ruleTo.getBody(), assignments.to, ruleFrom.getHeads(), assignments.from, assignments.groups, satisfied, false);
    // fromHeadSatisfied |= positiveModels(ruleTo.getHeads(), assignments.to, ruleFrom.getHeads(), assignments.from, assignments.groups, satisfied, false);
    // fromHeadSatisfied |= positiveModels(ruleFrom.getBody(), assignments.from, ruleFrom.getHeads(), assignments.from, assignments.groups, satisfied, true);
    // fromHeadSatisfied |= positiveModels(notMappedPieceLiterals, assignments.to, ruleFrom.getHeads(), assignments.from, assignments.groups, satisfied, false);

    if (fromHeadSatisfied)
        return restrainExtend(mappingDomain, ruleFrom, ruleTo, ruleToPiece, assignments);

    return true;
}

bool restrainExtendAssignment(const Literal &literalFrom, const Literal &literalTo,
    VariableAssignments &assignments)
{
    unsigned tupleSize = literalFrom.getTupleSize(); //Should be the same as literalTo.getTupleSize()

    for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
    {
        VTerm fromTerm = literalFrom.getTermAtPos(termIndex);
        VTerm toTerm = literalTo.getTermAtPos(termIndex);
    
        TermInfo fromInfo = getTermInfo(fromTerm, assignments, RelianceRuleRelation::From);
        TermInfo toInfo = getTermInfo(toTerm, assignments, RelianceRuleRelation::To);

        RelianceTermCompatible compatibleInfo;
        bool compatibleResult = termsEqual(fromInfo, toInfo, assignments, &compatibleInfo);

        if (compatibleResult)
        {
            continue;
        }
        else
        {
            if (compatibleInfo.type == RelianceTermCompatible::Types::Incompatible)
            {
                return false;
            }
            else
            {
                // We may not assign a universal variable to a null
                if ((toInfo.type == TermInfo::Types::Universal || fromInfo.type == TermInfo::Types::Universal) 
                    && compatibleInfo.constant < 0)
                {
                    return false;
                }

                makeCompatible(compatibleInfo, assignments.from, assignments.to, assignments.groups);
            }
        }
    }

    return true;
}

bool restrainExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo, const std::vector<Literal> &ruleToPiece,
    const VariableAssignments &assignments)
{
    unsigned headToStartIndex = (mappingDomain.size() == 0) ? 0 : mappingDomain.back() + 1;

    for (unsigned headToIndex = headToStartIndex; headToIndex < ruleToPiece.size(); ++headToIndex)
    {
        const Literal &literalTo = ruleToPiece[headToIndex];
        mappingDomain.push_back(headToIndex);

        for (unsigned headFromIndex = 0; headFromIndex < ruleFrom.getHeads().size(); ++headFromIndex)
        {
            const Literal &literalFrom =  ruleFrom.getHeads().at(headFromIndex);

            if (literalTo.getPredicate().getId() != literalFrom.getPredicate().getId())
                continue;

            VariableAssignments extendedAssignments = assignments;
            if (!restrainExtendAssignment(literalFrom, literalTo, extendedAssignments))
                continue;

            if (restrainCheck(mappingDomain, ruleFrom, ruleTo, ruleToPiece, extendedAssignments))
                return true;
        }

        mappingDomain.pop_back();
    }

    return false;
}

bool restrainReliance(const Rule &ruleFrom, unsigned variableCountFrom, const Rule &ruleTo, const std::vector<Literal> &ruleToPiece, unsigned variableCountTo)
{
    std::vector<unsigned> mappingDomain;
    VariableAssignments assignments(variableCountFrom, variableCountTo);

    return restrainExtend(mappingDomain, ruleFrom, ruleTo, ruleToPiece, assignments);

}

std::pair<RelianceGraph, RelianceGraph> computeRestrainReliances(std::vector<Rule> &rules)
{
    RelianceGraph result(rules.size()), resultTransposed(rules.size());

    std::vector<MarkResult> markedRules;
    markedRules.reserve(rules.size());
    std::vector<unsigned> variableCounts;
    variableCounts.reserve(rules.size());

    for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {
        const Rule &currentRule = rules[ruleIndex];

        unsigned variableCount = std::max(highestLiteralsId(currentRule.getHeads()), highestLiteralsId(currentRule.getBody()));
        variableCounts.push_back(variableCount + 1);
    }

    for (const Rule &currentRule : rules)
    {
        markedRules.push_back(markExistentialVariablesAndPieces(currentRule));
    }

    std::unordered_map<PredId_t, std::vector<size_t>> headFromMap, headToMap;

    for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {   
        const Rule &markedRule = markedRules[ruleIndex].singleRule;

        for (const Literal &currentLiteral : markedRule.getHeads())
        {
            bool containsExistentialVariable = false;
            for (size_t termIndex = 0; termIndex < currentLiteral.getTupleSize(); ++termIndex)
            {
                VTerm currentTerm = currentLiteral.getTermAtPos(termIndex);

                if ((int32_t)currentTerm.getId() < 0)
                {
                    containsExistentialVariable = true;
                    break;
                }
            }

            headFromMap[currentLiteral.getPredicate().getId()].push_back(ruleIndex);

            std::vector<size_t> &headToVector = headToMap[currentLiteral.getPredicate().getId()];
            if (containsExistentialVariable)
                headToVector.push_back(ruleIndex);
        }
    }

    std::unordered_set<uint64_t> proccesedPairs;
    for (auto iteratorFrom : headFromMap)
    {
        auto iteratorTo = headToMap.find(iteratorFrom.first);
        
        if (iteratorTo == headToMap.end())
            continue;

        for (size_t ruleFrom : iteratorFrom.second)
        {
            for (size_t ruleTo : iteratorTo->second)
            {
                uint64_t hash = ruleFrom * rules.size() + ruleTo;
                if (proccesedPairs.find(hash) != proccesedPairs.end())
                    continue;

                unsigned variableCountFrom = variableCounts[ruleFrom];
                unsigned variableCountTo = variableCounts[ruleTo];

                for (unsigned pieceIndex = 0; pieceIndex < markedRules[ruleTo].headPieces.size(); ++pieceIndex)
                {
                    if (restrainReliance(markedRules[ruleFrom].singleRule, variableCountFrom, markedRules[ruleTo].singleRule, markedRules[ruleTo].headPieces[pieceIndex], variableCountTo))
                    {
                        result.addEdge(ruleFrom, ruleTo);
                        resultTransposed.addEdge(ruleTo, ruleFrom);
                    
                        break;
                    }
                }
            }
        }
    }

    return std::make_pair(result, resultTransposed);
}