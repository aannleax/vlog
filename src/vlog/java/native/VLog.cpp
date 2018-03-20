#include <jni.h>

#include <vlog/concepts.h>
#include <vlog/edb.h>
#include <vlog/seminaiver.h>
#include <vlog/reasoner.h>
#include <vlog/utils.h>
#include <kognac/utils.h>
#include <kognac/logs.h>

#include <iostream>
#include <fstream>
#include <cstring>

class VLogInfo {
public:
    SemiNaiver *sn;
    Program *program;
    EDBLayer *layer;

    VLogInfo() {
	sn = NULL;
	program = NULL;
	layer = NULL;
    }

    ~VLogInfo() {
	if (layer != NULL) {
	    delete layer;
	    layer = NULL;
	}
	if (program != NULL) {
	    delete program;
	    program = NULL;
	}
	if (sn != NULL) {
	    delete sn;
	    sn = NULL;
	}
    }
};

static std::map<jint, struct VLogInfo *> vlogMap;

static bool logLevelSet = false;

// Utility method to convert java string to c++ string
std::string jstring2string(JNIEnv *env, jstring jstr) {
    const char *cstr = env->GetStringUTFChars(jstr, NULL);
    std::string str = std::string(cstr);
    env->ReleaseStringUTFChars(jstr, cstr);
    return str;
}

// Utility method to convert a literal id to a string.
std::string literalToString(VLogInfo *f, uint64_t literalid) {
    char supportText[MAX_TERM_SIZE];
    if (!f->layer->getDictText(literalid, supportText)) {
	std::string s = f->program->getFromAdditional((Term_t) literalid);
	if (s == std::string("")) {
	    s = "" + std::to_string(literalid >> 40) + "_"
		    + std::to_string((literalid >> 32) & 0377) + "_"
		    + std::to_string(literalid & 0xffffffff);
	}

	return s;
    }
    return std::string(supportText);
}

jint getVLogId(JNIEnv *env, jobject jobj) {
    jclass vlogcls = env->GetObjectClass(jobj);
    jfieldID fid = env->GetFieldID(vlogcls, "myVlog", "I");
    return env->GetIntField(jobj, fid);
}

VLogInfo *getVLogInfo(jint id) {
    auto inf = vlogMap.find(id);
    if (inf == vlogMap.end()) {
	return NULL;
    }
    return inf->second;
}

VLogInfo *getVLogInfo(JNIEnv *env, jobject obj) {
    return getVLogInfo(getVLogId(env, obj));
}

// Utility method to throw an exception
void throwException(JNIEnv *env, const char *exceptionName, const char *message) {

    jclass exClass = env->FindClass(exceptionName);
    if (exClass == NULL) {
	// Should throw an exception.
        return;
    }

    env->ThrowNew(exClass, message);
}

void throwNotStartedException(JNIEnv *env, const char *message) {
    throwException(env, "karmaresearch/vlog/NotStartedException", message);
}

void throwIOException(JNIEnv *env, const char *message) {
    throwException(env, "java/io/IOException", message);
}

void throwAlreadyStartedException(JNIEnv *env, const char *message) {
    throwException(env, "karmaresearch/vlog/AlreadyStartedException", message);
}

void throwEDBConfigurationException(JNIEnv *env, const char *message) {
    throwException(env, "karmaresearch/vlog/EDBConfigurationException", message);
}

// Converts a vector of Atoms into VLog representation.
std::vector<Literal> getVectorLiteral(JNIEnv *env, VLogInfo *f, jobjectArray h, Dictionary &dict) {
    std::vector<Literal> result;
    jsize sz = env->GetArrayLength(h);
    // For all atoms:
    for (int i = 0; i < sz; i++) {
	// First, get the atom.
	jobject atom = env->GetObjectArrayElement(h, (jsize) i);
	jclass cls = env->GetObjectClass(atom);

	// Get the predicate
	jmethodID getPredicateMethod = env->GetMethodID(cls, "getPredicate", "()Ljava/lang/String;");
	jstring jpred = (jstring) env->CallObjectMethod(atom, getPredicateMethod);
	std::string predicate = jstring2string(env, jpred);

	// Get the terms
	jmethodID getTermsMethod = env->GetMethodID(cls, "getTerms", "()[Lkarmaresearch/vlog/Term;");
	jobjectArray jterms = (jobjectArray) env->CallObjectMethod(atom, getTermsMethod);
	jsize vtuplesz = env->GetArrayLength(jterms);

	// Collect conversions from terms
	VTuple tuple((uint8_t) vtuplesz);
	std::vector<VTerm> t;

	// For each term:
	for (int j = 0; j < vtuplesz; j++) {
	    // First, get the term
	    jobject jterm = env->GetObjectArrayElement(jterms, (jsize) j);
	    jclass termcls = env->GetObjectClass(jterm);

	    // Get the name
	    jmethodID getNameMethod = env->GetMethodID(termcls, "getName", "()Ljava/lang/String;");
	    jstring jname = (jstring) env->CallObjectMethod(jterm, getNameMethod);
	    std::string name = jstring2string(env, jname);

	    // Get the type: constant or variable
	    jmethodID getTypeMethod = env->GetMethodID(termcls, "getTermType", "()Lkarmaresearch/vlog/Term$TermType;");
	    jobject jtype = env->CallObjectMethod(jterm, getTypeMethod);
	    jclass enumClass = env->GetObjectClass(jtype);
	    jmethodID getOrdinalMethod = env->GetMethodID(enumClass, "ordinal", "()I");
	    jint type = (jint) env->CallIntMethod(jtype, getOrdinalMethod);

	    // For now, we only have two choices.
	    if (type != 0) {
		// Variable
		t.push_back(VTerm((uint8_t) dict.getOrAdd(name), 0));
	    } else {
		// Constant
		// name = program->rewriteRDFOWLConstants(name);
		uint64_t dictTerm;
		if (!f->layer->getDictNumber(name.c_str(), name.size(), dictTerm)) {
		    //Get an ID from the temporary dictionary
		    dictTerm = f->program->getOrAddToAdditional(name);
		}
		t.push_back(VTerm(0, dictTerm));
	    }
	}
	int pos = 0;
	for (std::vector<VTerm>::iterator itr = t.begin(); itr != t.end(); ++itr) {
	    tuple.set(*itr, pos++);
	}

	uint8_t adornment = Predicate::calculateAdornment(tuple);

	int64_t predid = f->program->getOrAddPredicate(predicate, (uint8_t) vtuplesz);
	if (predid < 0) {
	    // TODO: throw something
	}

	Predicate pred((PredId_t) predid, adornment, f->layer->doesPredExists((PredId_t) predid) ? EDB : IDB, (uint8_t) vtuplesz);

	Literal literal(pred, tuple);
	result.push_back(literal);
    }
    return result;
}

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    setLogLevel
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_karmaresearch_vlog_VLog_setLogLevel(JNIEnv *env, jobject obj, jstring level) {
    std::string ll = jstring2string(env, level);
    if (ll == "debug") {
        Logger::setMinLevel(DEBUGL);
    } else if (ll == "info") {
        Logger::setMinLevel(INFOL);
    } else if (ll == "warning") {
        Logger::setMinLevel(WARNL);
    } else if (ll == "error") {
        Logger::setMinLevel(ERRORL);
    } else {
        Logger::setMinLevel(INFOL);
    }
    logLevelSet = true;
}

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    start
 * Signature: (Ljava/lang/String;Z)V
 */
JNIEXPORT void JNICALL Java_karmaresearch_vlog_VLog_start(JNIEnv *env, jobject obj, jstring rawconf, jboolean isfile) {
    jint id = getVLogId(env, obj);
    VLogInfo *f = getVLogInfo(id);
    if (f != NULL) {
	throwAlreadyStartedException(env, "VLog is already started");
	return;
    }

    if (! logLevelSet) {
	Logger::setMinLevel(INFOL);
    }

    std::string crawconf = jstring2string(env, rawconf);
    if (isfile) {
	if (! Utils::exists(crawconf)) {
	    throwIOException(env, ("File " + crawconf + " does not exist").c_str());
	    return;
	}
    }

    f = new VLogInfo();
    try {
	EDBConf conf(crawconf.c_str(), isfile);

	try {
	    f->layer = new EDBLayer(conf, false);
	} catch(std::string s) {
	    throwIOException(env, s.c_str());
	    return;
	}
	f->program = new Program(f->layer->getNTerms(), f->layer);
    } catch(std::string s) {
	throwEDBConfigurationException(env, s.c_str());
    }
    vlogMap[id] = f;
}

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    stop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_karmaresearch_vlog_VLog_stop(JNIEnv *env, jobject obj) {
    jint id = getVLogId(env, obj);
    auto inf = vlogMap.find(id);
    if (inf == vlogMap.end()) {
	return;
    }
    delete inf->second;
    vlogMap.erase(inf);
}

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    addData
 * Signature: (Ljava/lang/String;[[Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_karmaresearch_vlog_VLog_addData(JNIEnv *env, jobject obj, jstring jpred, jobjectArray data) {
    jint id = getVLogId(env, obj);
    VLogInfo *f = getVLogInfo(id);
    if (f == NULL) {
	f = new VLogInfo();
	vlogMap[id] = f;
    }

    if (f->layer == NULL) {
	EDBConf conf("", false);
	f->layer = new EDBLayer(conf, false);
    }

    std::string pred = jstring2string(env, jpred);

    if (f->program != NULL) {
	if (f->program->getNRules() > 0) {
	    throwEDBConfigurationException(env, "Cannot add data if there already are rules");
	    return;
	}
	delete f->program;
	f->program = NULL;
    }

    if (data == NULL) {
	throwEDBConfigurationException(env, "null data");
	return;
    }
    jsize nrows = env->GetArrayLength(data);
    // For all rows:
    std::vector<std::vector<std::string>> values;
    for (int i = 0; i < nrows; i++) {
	// First, get the atom.
	std::vector<std::string> value;
	jobjectArray atom = (jobjectArray) env->GetObjectArrayElement(data, (jsize) i);
	if (atom == NULL) {
	    throwEDBConfigurationException(env, "null data");
	    return;
	}
	jint arity = env->GetArrayLength(atom);
	for (int j = 0; j < arity; j++) {
	    jstring v = (jstring) env->GetObjectArrayElement(atom, (jsize) j);
	    if (v == NULL) {
		throwEDBConfigurationException(env, "null data");
		return;
	    }
	    value.push_back(jstring2string(env, v));
	}
	values.push_back(value);
    }

    try {
	f->layer->addInmemoryTable(pred, values);
    } catch(std::string s) {
	throwEDBConfigurationException(env, s.c_str());
	return;
    }

    f->program = new Program(f->layer->getNTerms(), f->layer);
}


/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    getPredicateId
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_karmaresearch_vlog_VLog_getPredicateId(JNIEnv *env, jobject obj, jstring p) {

    VLogInfo *f = getVLogInfo(env, obj);
    if (f == NULL || f->program == NULL) {
	throwNotStartedException(env, "VLog is not started yet");
	return (jint) -1;
    }

    //Transform the string into a C++ string
    std::string predName = jstring2string(env, p);

    // TODO: fix this: this might create a new predicate if it does not exist.
    // There should be a way to just do a lookup???
    Predicate pred = f->program->getPredicate(predName);

    return (jint) pred.getId();
}

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    getPredicate
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_karmaresearch_vlog_VLog_getPredicate(JNIEnv *env, jobject obj, jint predid) {
    VLogInfo *f = getVLogInfo(env, obj);
    if (f == NULL || f->program == NULL) {
	throwNotStartedException(env, "VLog is not started yet");
	return NULL;
    }

    std::string pred = f->program->getPredicateName((PredId_t) predid);
    if (pred == std::string("")) {
	return NULL;
    }
    return env->NewStringUTF(pred.c_str());
}

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    getConstantId
 * Signature: (Ljava/lang/String;)J
 */
JNIEXPORT jlong JNICALL Java_karmaresearch_vlog_VLog_getConstantId(JNIEnv *env, jobject obj, jstring literal) {

    VLogInfo *f = getVLogInfo(env, obj);
    if (f == NULL || f->layer == NULL) {
	throwNotStartedException(env, "VLog is not started yet");
	return (jint) -1;
    }

    const char *cliteral = env->GetStringUTFChars(literal, 0);
    uint64_t value;
    jlong retval = -1;
    if (f->layer->getDictNumber(cliteral, strlen(cliteral), value)) {
	retval = value;
    }
    // TODO: lookup in additional, and deal with results of chases.
    env->ReleaseStringUTFChars(literal, cliteral);
    return retval;
}

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    getConstant
 * Signature: (J)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_karmaresearch_vlog_VLog_getConstant(JNIEnv *env, jobject obj, jlong literalid) {
    VLogInfo *f = getVLogInfo(env, obj);
    if (f == NULL || f->program == NULL) {
	throwNotStartedException(env, "VLog is not started yet");
	return NULL;
    }
    return env->NewStringUTF(literalToString(f, literalid).c_str());
}

static TupleIterator *getQueryIter(JNIEnv *env, jobject obj, PredId_t p, jlongArray els) {
    VLogInfo *f = getVLogInfo(env, obj);
    if (f == NULL || f->program == NULL) {
	throwNotStartedException(env, "VLog is not started yet");
	return NULL;
    }

    // Create a VLog query from the parameters.
    Predicate pred = f->program->getPredicate((PredId_t) p);
    jsize sz = env->GetArrayLength(els);
    VTuple tuple((uint8_t) sz);
    jlong *e = env->GetLongArrayElements(els, NULL);
    for (int i = 0; i < sz; i++) {
	jlong v = e[i];
	uint8_t varId = 0;
	uint64_t val = 0;
	if (v < 0) {
	    varId = (uint8_t) -v;
	} else {
	    val = v;
	}
	VTerm vterm(varId, val);
	tuple.set(vterm, i);
    }
    env->ReleaseLongArrayElements(els, e, JNI_ABORT);
    Literal query(pred, tuple);

    // Now create an iterator over the query result.
    TupleIterator *iter = NULL;
    Reasoner r((uint64_t) 0);
    if (pred.getType() == EDB) {
	iter = r.getEDBIterator(query, NULL, NULL, *(f->layer), false, NULL);
    } else if (f->sn != NULL) {
	iter = r.getIteratorWithMaterialization(f->sn, query, false, NULL);
    } else {
	// No materialization yet, but non-EDB predicate ... so, empty.
	TupleTable *table = new TupleTable(sz);
	std::shared_ptr<TupleTable> pt(table);
	iter = new TupleTableItr(pt);
    }
    return iter;
}

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    query
 * Signature: (I[J)Lkarmaresearch/vlog/QueryResultEnumeration;
 */
JNIEXPORT jobject JNICALL Java_karmaresearch_vlog_VLog_query(JNIEnv * env, jobject obj, jint p, jlongArray els ) {
    TupleIterator *iter = getQueryIter(env, obj, (PredId_t) p, els);
    jclass jcls=env->FindClass("karmaresearch/vlog/QueryResultEnumeration");
    jmethodID mID = env->GetMethodID(jcls, "<init>", "(J)V");
    jobject jobj = env->NewObject(jcls, mID, (jlong) iter);

    return jobj;
}


/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    setRules
 * Signature: ([Lkarmaresearch/vlog/Rule;Lkarmaresearch/vlog/VLog/RuleRewriteStrategy;)V
 */
JNIEXPORT void JNICALL Java_karmaresearch_vlog_VLog_setRules(JNIEnv *env, jobject obj, jobjectArray rules, jobject rewriteHeads) {
    VLogInfo *f = getVLogInfo(env, obj);
    if (f == NULL || f->program == NULL) {
	throwNotStartedException(env, "VLog is not started yet");
	return;
    }
    if (rules != NULL) {
	// Create a new program, to remove any left-overs from old rule stuff
	delete f->program;
	f->program = new Program(f->layer->getNTerms(), f->layer);

	// Get rewrite flag
	jclass enumClass = env->GetObjectClass(rewriteHeads);
	jmethodID getOrdinalMethod = env->GetMethodID(enumClass, "ordinal", "()I");
	jint rewrite = (jint) env->CallIntMethod(rewriteHeads, getOrdinalMethod);

	// Get number of rules
	jsize sz = env->GetArrayLength(rules);

	// For each rule:
	for (int i = 0; i < sz; i++) {
	    Dictionary dictVariables; // temporary dictionary for variables.

	    jobject rule = env->GetObjectArrayElement(rules, (jsize) i);
	    jclass cls = env->GetObjectClass(rule);
	    jmethodID getHeadMethod = env->GetMethodID(cls, "getHead", "()[Lkarmaresearch/vlog/Atom;");
	    jmethodID getBodyMethod = env->GetMethodID(cls, "getBody", "()[Lkarmaresearch/vlog/Atom;");

	    // Get fields: head and body.
	    jobjectArray head = (jobjectArray) env->CallObjectMethod(rule, getHeadMethod); 
	    jobjectArray body = (jobjectArray) env->CallObjectMethod(rule, getBodyMethod); 

	    // Convert them into internal VLog format.
	    std::vector<Literal> vhead = getVectorLiteral(env, f, head, dictVariables);
	    std::vector<Literal> vbody = getVectorLiteral(env, f, body, dictVariables);

	    // And add the rule.
	    f->program->addRule(vhead, vbody, rewrite != 0);
	}
    }
}

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    setRulesFile
 * Signature: (Ljava/lang/String;Lkarmaresearch/vlog/VLog/RuleRewriteStrategy;)V
 */
JNIEXPORT void JNICALL Java_karmaresearch_vlog_VLog_setRulesFile(JNIEnv *env, jobject obj, jstring fn, jobject rewriteHeads) {
    VLogInfo *f = getVLogInfo(env, obj);
    if (f == NULL || f->program == NULL) {
	throwNotStartedException(env, "VLog is not started yet");
	return;
    }

    delete f->program;
    f->program = new Program(f->layer->getNTerms(), f->layer);

    // Get rewrite flag
    jclass enumClass = env->GetObjectClass(rewriteHeads);
    jmethodID getOrdinalMethod = env->GetMethodID(enumClass, "ordinal", "()I");
    jint rewrite = (jint) env->CallIntMethod(rewriteHeads, getOrdinalMethod);

    //Transform the string into a C++ string
    std::string fileName = jstring2string(env, fn);

    if (! Utils::exists(fileName)) {
	throwIOException(env, ("File " + fileName + " does not exist").c_str());
	return;
    }
    f->program->readFromFile(fileName, rewrite != 0);
}

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    materialize
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_karmaresearch_vlog_VLog_materialize(JNIEnv *env, jobject obj, jboolean skolem) {
    VLogInfo *f = getVLogInfo(env, obj);
    if (f == NULL || f->program == NULL) {
	throwNotStartedException(env, "VLog is not started yet");
	return;
    }

    if (f->sn != NULL) {
	delete f->sn;
    }

    f->sn = new SemiNaiver(f->program->getAllRules(), *(f->layer), f->program, true, true, false, ! (bool) skolem, -1, false);
    LOG(INFOL) << "Starting full materialization";
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    f->sn->run();
    std::chrono::duration<double> sec = std::chrono::system_clock::now() - start;
    LOG(INFOL) << "Runtime materialization = " << sec.count() * 1000 << " milliseconds";
    f->sn->printCountAllIDBs("");
}

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    writePredicateToCsv
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_karmaresearch_vlog_VLog_writePredicateToCsv(JNIEnv *env, jobject obj, jstring jpred, jstring jfile) {
    VLogInfo *f = getVLogInfo(env, obj);
    if (f == NULL || f->program == NULL) {
	throwNotStartedException(env, "VLog is not started yet");
	return;
    }
    if (f->sn == NULL) {
	throwNotStartedException(env, "Materialization has not run yet");
	return;
    }
    jint predId = Java_karmaresearch_vlog_VLog_getPredicateId(env, obj, jpred);
    std::string fn = jstring2string(env, jfile);
    try {
	f->sn->storeOnFile(fn, (PredId_t) predId, true, 0, true);
    } catch(std::string s) {
	throwIOException(env, s.c_str());
    }
}

/*
 * Class:     karmaresearch_vlog_VLog
 * Method:    queryToCsv
 * Signature: (I[JLjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_karmaresearch_vlog_VLog_queryToCsv(JNIEnv *env, jobject obj, jint pred, jlongArray q, jstring jfile) {
    char buffer[65536];
    VLogInfo *f = getVLogInfo(env, obj);
    if (f == NULL || f->program == NULL) {
	throwNotStartedException(env, "VLog is not started yet");
	return;
    }
    std::string fn = jstring2string(env, jfile);
    std::ofstream streamout(fn);
    if (streamout.fail()) {
        throwIOException(env, ("Could not open " + fn + " for writing").c_str());
	return;
    }
    TupleIterator *iter = getQueryIter(env, obj, (PredId_t) pred, q);
    if (iter == NULL) {
	return;
    }
    size_t sz = iter->getTupleSize();
    while (iter->hasNext()) {
	iter->next();
	for (int i = 0; i < sz; i++) {
	    if (i != 0) {
		streamout << ",";
	    }
	    std::string v = literalToString(f, iter->getElementAt(i));
	    streamout << VLogUtils::csvString(v);
	}
	streamout << std::endl;
    }
    delete iter;
    streamout.close();
}

/*
 * Class:     karmaresearch_vlog_QueryResultEnumeration
 * Method:    hasMoreElements
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_karmaresearch_vlog_QueryResultEnumeration_hasMoreElements(JNIEnv *env, jobject obj, jlong ref) {
    TupleIterator *iter = (TupleIterator *) ref;
    return (jboolean) iter->hasNext();
}

/*
 * Class:     karmaresearch_vlog_QueryResultEnumeration
 * Method:    nextElement
 * Signature: (J)[J
 */
JNIEXPORT jlongArray JNICALL Java_karmaresearch_vlog_QueryResultEnumeration_nextElement(JNIEnv *env, jobject obj, jlong ref) {
    TupleIterator *iter = (TupleIterator *) ref;
    size_t sz = iter->getTupleSize();
    iter->next();
    jlong res[16];
    for (int i = 0; i < sz; i++) {
	res[i] = iter->getElementAt(i);
    }
    jlongArray outJNIArray = env->NewLongArray(sz);
    if (NULL == outJNIArray) return NULL;
    env->SetLongArrayRegion(outJNIArray, 0 , sz, res);
    return outJNIArray;
}

/*
 * Class:     karmaresearch_vlog_QueryResultEnumeration
 * Method:    cleanup
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_karmaresearch_vlog_QueryResultEnumeration_cleanup(JNIEnv *env, jobject obj, jlong ref) {
    TupleIterator *iter = (TupleIterator *) ref;
    delete iter;
}


#ifdef __cplusplus
}
#endif