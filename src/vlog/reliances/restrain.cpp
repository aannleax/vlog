#include "vlog/reliances/reliances.h"

#include <vector>
#include <utility>
#include <list>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

// struct RestrainPiece
// {
//     std::vector<unsigned> LiteralIndices;
// };

// struct MarkResult
// {
//     MarkResult(uint32_t ruleId, const std::vector<Literal> &body, const std::vector<Literal> &heads, const std::vector<std::vector<Literal>> &headSplit) 
//         : singleRule(ruleId, heads, body), headPieces(headSplit) {}

//     Rule singleRule;
//     std::vector<std::vector<Literal>> headPieces;
// };

// void createPiece(const std::vector<Literal> &heads, unsigned currentLiteralIndex, std::vector<bool> &literalsInPieces, std::vector<Literal> &result)
// {
//     bool validPiece = false; //Piece has to contain an existential variable

//     const Literal &currentLiteral = heads[currentLiteralIndex];
//     literalsInPieces[currentLiteralIndex] = true;

//     for (unsigned termIndex = 0; termIndex < currentLiteral.getTupleSize(); ++termIndex)
//     {
//         VTerm currentTerm = currentLiteral.getTermAtPos(termIndex);

//         if ((int)currentTerm.getId() >= 0)
//             continue;

//         validPiece = true;

//         for (unsigned searchIndex = currentLiteralIndex + 1; searchIndex < heads.size(); ++searchIndex)
//         {
//             if (literalsInPieces[searchIndex])
//                 continue;

//             const Literal &currentSearchedLiteral = heads[searchIndex];
//             for (unsigned searchTermIndex = 0; searchTermIndex < currentSearchedLiteral.getTupleSize(); ++searchTermIndex)
//             {
//                 VTerm currentSearchedTerm = currentSearchedLiteral.getTermAtPos(searchTermIndex);
            
//                 if (currentTerm.getId() == currentSearchedTerm.getId())
//                 {
//                     createPiece(heads, searchIndex, literalsInPieces, result);
//                     break;
//                 }
//             }
//         }
//     }

//     if (validPiece)
//         result.push_back(currentLiteral);
// }

// MarkResult markExistentialVariablesAndPieces(const Rule &rule)
// {
//     uint32_t ruleId = rule.getId();
//     std::vector<Literal> body = rule.getBody();
//     std::vector<Literal> heads;

//     std::vector<Var_t> existentialVariables = rule.getExistentialVariables();

//     for (const Literal &literal: rule.getHeads())
//     {
//         VTuple *tuple = new VTuple(literal.getTupleSize());

//         for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
//         {
//             VTerm currentTerm = literal.getTermAtPos(termIndex);;

//             if (currentTerm.getId() > 0 && std::find(existentialVariables.begin(), existentialVariables.end(), currentTerm.getId()) != existentialVariables.end())
//             {
//                 currentTerm.setId(-currentTerm.getId());
//             }

//             tuple->set(currentTerm, termIndex);
//         }

//         heads.push_back(Literal(literal.getPredicate(), *tuple, literal.isNegated()));
//     }

//     std::vector<std::vector<Literal>> headsSplit;
//     std::vector<bool> literalsInPieces;
//     literalsInPieces.resize(heads.size(), false);

//     for (unsigned literalIndex = 0; literalIndex < heads.size(); ++literalIndex)
//     {
//         const Literal &currentLiteral = heads[literalIndex];
        
//         if (literalsInPieces[literalIndex])
//             continue;

//         std::vector<Literal> newPiece;
//         createPiece(heads, literalIndex, literalsInPieces, newPiece);
//         if (newPiece.size() > 0)
//             headsSplit.push_back(newPiece);
//     }

//     return MarkResult(ruleId, body, heads, headsSplit);
// }

bool restrainExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo, 
    const VariableAssignments &assignments);

bool checkUnmappedExistentialVariables(const std::vector<Literal> &literals, 
    const VariableAssignments &assignments)
{
    for (const Literal &literal : literals)
    {
        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);

            if ((int32_t)currentTerm.getId() < 0)
            {
                if (assignments.getGroupId((int32_t)currentTerm.getId(), RelianceRuleRelation::To) != NOT_ASSIGNED
                    || assignments.getConstant((int32_t)currentTerm.getId(), RelianceRuleRelation::To) != NOT_ASSIGNED)
                    return false;
            }                
        }
    }

    return true;
}

bool restrainCheck(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments)
{
    unsigned nextInDomainIndex = 0;
    const std::vector<Literal> toHeadLiterals = ruleTo.getHeads();
    std::vector<Literal> notMappedHeadLiterals;
    notMappedHeadLiterals.reserve(ruleTo.getHeads().size());
    for (unsigned headIndex = 0; headIndex < toHeadLiterals.size(); ++headIndex)
    {
        if (headIndex == mappingDomain[nextInDomainIndex])
        {
            if (nextInDomainIndex < mappingDomain.size() - 1)
            {
                ++nextInDomainIndex;
            }

            continue;
        }

        notMappedHeadLiterals.push_back(toHeadLiterals[headIndex]);
    }

    if (!assignments.hasMappedExistentialVariable)
        return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments);

    if (!checkUnmappedExistentialVariables(notMappedHeadLiterals, assignments))
    {
        return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments);
    }

    std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> existentialMappings;
    std::vector<unsigned> satisfied;
    satisfied.resize(ruleTo.getHeads().size(), 0);
    prepareExistentialMappings(ruleTo.getHeads(), RelianceRuleRelation::To, assignments, existentialMappings);

    bool toHeadSatisfied =
        relianceModels(ruleTo.getBody(), RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);
            
    if (toHeadSatisfied && ruleTo.isExistential())
    {
        toHeadSatisfied = checkConsistentExistential(existentialMappings);
    }

    if (toHeadSatisfied)
        return false;

    prepareExistentialMappings(ruleTo.getHeads(), RelianceRuleRelation::To, assignments, existentialMappings);
    satisfied.clear();
    satisfied.resize(ruleTo.getHeads().size(), 0);

    bool alternativeMatchAlreadyPresent = relianceModels(ruleTo.getBody(), RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, false, false);
    alternativeMatchAlreadyPresent |= relianceModels(ruleTo.getHeads(), RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, true, false);
    alternativeMatchAlreadyPresent |= relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, false, false);
    alternativeMatchAlreadyPresent |= relianceModels(notMappedHeadLiterals, RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, false, false);

    if (alternativeMatchAlreadyPresent && ruleTo.isExistential())
    {
        alternativeMatchAlreadyPresent = checkConsistentExistential(existentialMappings);
    }

    if (alternativeMatchAlreadyPresent)
        return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments);

    prepareExistentialMappings(ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, existentialMappings);
    satisfied.clear();
    satisfied.resize(ruleFrom.getHeads().size(), 0);

    bool fromHeadSatisfied = relianceModels(ruleTo.getBody(), RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);
    fromHeadSatisfied |= relianceModels(ruleTo.getHeads(), RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings, true);
    fromHeadSatisfied |= relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);
    fromHeadSatisfied |= relianceModels(notMappedHeadLiterals, RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);

    if (fromHeadSatisfied && ruleFrom.isExistential())
    {
        fromHeadSatisfied = checkConsistentExistential(existentialMappings);
    }

    if (fromHeadSatisfied)
        return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments);

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
    
        TermInfo fromInfo = getTermInfoUnify(fromTerm, assignments, RelianceRuleRelation::From);
        TermInfo toInfo = getTermInfoUnify(toTerm, assignments, RelianceRuleRelation::To);

         // We may not assign any universal variable to a null
        if ((toInfo.type == TermInfo::Types::Universal || fromInfo.type == TermInfo::Types::Universal) 
            && (fromInfo.constant < 0 || toInfo.constant < 0))
            return false;

        if (!unifyTerms(fromInfo, toInfo, assignments))
            return false;

        if (toInfo.type == TermInfo::Types::Existential)
            assignments.hasMappedExistentialVariable = true;
    }

    assignments.finishGroupAssignments();

    return true;
}

bool restrainExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments)
{
    unsigned headToStartIndex = (mappingDomain.size() == 0) ? 0 : mappingDomain.back() + 1;

    for (unsigned headToIndex = headToStartIndex; headToIndex < ruleTo.getHeads().size(); ++headToIndex)
    {
        const Literal &literalTo = ruleTo.getHeads()[headToIndex];
        mappingDomain.push_back(headToIndex);

        for (unsigned headFromIndex = 0; headFromIndex < ruleFrom.getHeads().size(); ++headFromIndex)
        {
            const Literal &literalFrom =  ruleFrom.getHeads().at(headFromIndex);

            if (literalTo.getPredicate().getId() != literalFrom.getPredicate().getId())
                continue;

            VariableAssignments extendedAssignments = assignments;
            if (!restrainExtendAssignment(literalFrom, literalTo, extendedAssignments))
                continue;

            if (restrainCheck(mappingDomain, ruleFrom, ruleTo, extendedAssignments))
                return true;
        }

        mappingDomain.pop_back();
    }

    return false;
}

bool restrainReliance(const Rule &ruleFrom, unsigned variableCountFrom, const Rule &ruleTo, unsigned variableCountTo)
{
    std::vector<unsigned> mappingDomain;
    VariableAssignments assignments(variableCountFrom, variableCountTo);

    return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments);
}


std::pair<SimpleGraph, SimpleGraph> computeRestrainReliances(std::vector<Rule> &rules)
{
    SimpleGraph result(rules.size()), resultTransposed(rules.size());
    
    std::vector<Rule> markedRules;
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
        Rule markedRule = markExistentialVariables(currentRule);
        markedRules.push_back(markedRule);
    }

    std::unordered_map<PredId_t, std::vector<size_t>> headFromMap, headToMap;

    for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {   
        const Rule &markedRule = markedRules[ruleIndex];

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
                if (ruleFrom == 2172 && ruleTo == 2128)
                    int x = 0;

                uint64_t hash = ruleFrom * rules.size() + ruleTo;
                if (proccesedPairs.find(hash) != proccesedPairs.end())
                    continue;
                proccesedPairs.insert(hash);

                unsigned variableCountFrom = variableCounts[ruleFrom];
                unsigned variableCountTo = variableCounts[ruleTo];

                if (restrainReliance(markedRules[ruleFrom], variableCountFrom, markedRules[ruleTo], variableCountTo))
                {
                    result.addEdge(ruleFrom, ruleTo);
                    resultTransposed.addEdge(ruleTo, ruleFrom);
                }
            }
        }
    }

    return std::make_pair(result, resultTransposed);
}