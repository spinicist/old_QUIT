/*
 *  Model.cpp
 *
 *  Created by Tobias Wood on 14/11/2012.
 *  Copyright (c) 2013 Tobias Wood.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "Model.h"

//******************************************************************************
#pragma mark Signal Functors
//******************************************************************************
const string Signal::to_string(const Components& c) {
	switch (c) {
		case Components::One: return "1";
		case Components::Two: return "2";
		case Components::Three: return "3";
	}
};

Signal::Signal(const ArrayXd &flip, const double TR) : m_flip(flip), m_TR(TR) {}
ostream& operator<<(ostream& os, const Signal& s) {
	s.write(os);
	return os;
}

SPGRSimple::SPGRSimple(const ArrayXd &flip, const double TR) : Signal(flip, TR) {}
void SPGRSimple::write(ostream &os) const {
	os << "SPGR Simple" << endl;
	os << "TR: " << m_TR << endl;
	os << "Angles: " << (m_flip * 180. / M_PI).transpose() << endl;
}
ArrayXd SPGRSimple::signal(const Components nC, const VectorXd &p, const double B1) const {
	switch (nC) {
		case (Components::One) : return SigMag(One_SPGR(p, m_flip, m_TR, B1));
		case (Components::Two) : return SigMag(Two_SPGR(p, m_flip, m_TR, B1));
		case (Components::Three) : return SigMag(Three_SPGR(p, m_flip, m_TR, B1));
	}
}

SPGRFinite::SPGRFinite(const ArrayXd &flip, const double TR, const double Trf, const double TE) : Signal(flip, TR), m_Trf(Trf), m_TE(TE) {}
void SPGRFinite::write(ostream &os) const {
	os << "SPGR Finite" << endl;
	os << "TR: " << m_TR << "\tTrf: " << m_Trf << "\tTE: " << m_TE << endl;
	os << "Angles: " << (m_flip * 180. / M_PI).transpose() << endl;
}
ArrayXd SPGRFinite::signal(const Components nC, const VectorXd &p, const double B1) const {
	switch (nC) {
		case (Components::One) : return SigMag(One_SSFP_Finite(p, m_flip, true, m_TR, m_Trf, m_TE, 0, B1));
		case (Components::Two) : return SigMag(Two_SSFP_Finite(p, m_flip, true, m_TR, m_Trf, m_TE, 0, B1));
		case (Components::Three) : return SigMag(Three_SSFP_Finite(p, m_flip, true, m_TR, m_Trf, m_TE, 0, B1));
	}
}


SSFPSimple::SSFPSimple(const ArrayXd &flip, const double TR, const ArrayXd &phases) : Signal(flip, TR), m_phases(phases) {}
void SSFPSimple::write(ostream &os) const {
	os << "SSFP Simple" << endl;
	os << "TR: " << m_TR << "\tPhases: " << (m_phases * 180. / M_PI).transpose() << endl;
	os << "Angles: " << (m_flip * 180. / M_PI).transpose() << endl;
}
size_t SSFPSimple::size() const { return m_flip.rows() * m_phases.rows(); }
ArrayXd SSFPSimple::signal(const Components nC, const VectorXd &p, const double B1) const {
	ArrayXd s(size());
	ArrayXd::Index start = 0;
	for (ArrayXd::Index i = 0; i < m_phases.rows(); i++) {
		switch (nC) {
			case (Components::One) : s.segment(start, m_flip.rows()) = SigMag(One_SSFP(p, m_flip, m_TR, m_phases(i), B1)); break;
			case (Components::Two) : s.segment(start, m_flip.rows()) = SigMag(Two_SSFP(p, m_flip, m_TR, m_phases(i), B1)); break;
			case (Components::Three) : s.segment(start, m_flip.rows()) = SigMag(Three_SSFP(p, m_flip, m_TR, m_phases(i), B1)); break;
		}
		start += m_flip.rows();
	}
	return s;
}

SSFPFinite::SSFPFinite(const ArrayXd &flip, const double TR, const double Trf, const ArrayXd &phases) : SSFPSimple(flip, TR, phases), m_Trf(Trf) {}
void SSFPFinite::write(ostream &os) const {
	os << "SSFP Finite" << endl;
	os << "TR: " << m_TR << "\tTrf: " << m_Trf << "\tPhases: " << (m_phases * 180. / M_PI).transpose() << endl;
	os << "Angles: " << (m_flip * 180. / M_PI).transpose() << endl;
}
ArrayXd SSFPFinite::signal(const Components nC, const VectorXd &p, const double B1) const {
	ArrayXd s(size());
	ArrayXd::Index start = 0;
	for (ArrayXd::Index i = 0; i < m_phases.rows(); i++) {
		switch (nC) {
			case (Components::One) : s.segment(start, m_flip.rows()) = SigMag(One_SSFP_Finite(p, m_flip, false, m_TR, m_Trf, 0., m_phases(i), B1)); break;
			case (Components::Two) : s.segment(start, m_flip.rows()) = SigMag(Two_SSFP_Finite(p, m_flip, false, m_TR, m_Trf, 0., m_phases(i), B1)); break;
			case (Components::Three) : s.segment(start, m_flip.rows()) = SigMag(Three_SSFP_Finite(p, m_flip, false, m_TR, m_Trf, 0., m_phases(i), B1)); break;
		}
		start += m_flip.rows();
	}
	return s;
}

//******************************************************************************
#pragma mark Model Class
//******************************************************************************
const string Model::to_string(const FieldStrength& f) {
	static const string f3{"3T"}, f7{"7T"}, fu{"User"};
	switch (f) {
		case FieldStrength::Three: return f3;
		case FieldStrength::Seven: return f7;
		case FieldStrength::User: return fu;
	}
}
const string Model::to_string(const Scaling &p) {
	static const string sn{"None"}, snm{"Normalised to Mean"};
	switch (p) {
		case Scaling::None: return sn;
		case Scaling::NormToMean: return snm;
	}
}


Model::Model(const Signal::Components c, const Scaling s) : m_nC(c), m_scaling(s) {}

ostream& operator<<(ostream &os, const Model& m) {
	os << "Model Parameters: " << m.nParameters() << endl;
	os << "Names:\t"; for (auto& n : m.names()) os << n << "\t"; os << endl;
	os << "Signals: " << m.m_signals.size() << "\tTotal size: " << m.size() << endl;
	for (auto& sig : m.m_signals)
		os << *sig;
	return os;
}

const size_t Model::size() const {
	size_t sz = 0;
	for (auto& sig : m_signals)
		sz += sig->size();
	return sz;
}

const ArrayXd Model::signal(const VectorXd &p, const double B1) const {
	ArrayXd result(size());
	size_t start = 0;
	for (auto &sig : m_signals) {
		ArrayXd thisResult = sig->signal(m_nC, p, B1);
		switch (m_scaling) {
			case Scaling::None :       break;
			case Scaling::NormToMean : thisResult /= thisResult.mean();
		}
		result.segment(start, sig->size()) = thisResult;
		start += sig->size();
	}
	return result;
}

const size_t Model::nParameters() const {
	switch (m_nC) {
		case Signal::Components::One: return 3;
		case Signal::Components::Two: return 7;
		case Signal::Components::Three: return 10;
	}
}

const vector<string> &Model::names() const {
	static vector<string> n1 {"T1", "T2", "f0"},
	                      n2 {"T1_a", "T2_a", "T1_b", "T2_b", "tau_a", "f_a", "f0"},
				          n3 {"T1_a", "T2_a", "T1_b", "T2_b", "T1_c", "T2_c", "tau_a", "f_a", "f_c", "f0"};
	
	switch (m_nC) {
		case Signal::Components::One: return n1;
		case Signal::Components::Two: return n2;
		case Signal::Components::Three: return n3;
	}
}

const ArrayXXd Model::bounds(const FieldStrength f) const {
	size_t nP = nParameters();
	ArrayXXd b(nP, 2);
	switch (f) {
		case FieldStrength::Three:
			switch (m_nC) {
				case Signal::Components::One:   b.block(0, 0, nP - 1, 2) << 0.1, 4.0, 0.01, 1.5; break;
				case Signal::Components::Two:   b.block(0, 0, nP - 1, 2) << 0.200, 0.350, 0.005, 0.015, 0.700, 2.000, 0.050, 0.120, 0.050, 0.200, 0.0, 0.5; break;
				case Signal::Components::Three: b.block(0, 0, nP - 1, 2) << 0.200, 0.350, 0.005, 0.015, 0.700, 2.000, 0.050, 0.120, 3.500, 7.000, 3.000, 7.000, 0.050, 0.200, 0, 0.5, 0, 1.0; break;
			} break;
		case FieldStrength::Seven:
			switch (m_nC) {
				case Signal::Components::One:   b.block(0, 0, nP - 1, 2) << 0.1, 4.0, 0.01, 2.0; break;
				case Signal::Components::Two:   b.block(0, 0, nP - 1, 2) << 0.1, 0.5, 0.001, 0.025, 1.0, 4.0, 0.04, 0.08, 0.01, 0.25, 0.001, 1.0; break;
				case Signal::Components::Three: b.block(0, 0, nP - 1, 2) << 0.1, 0.5, 0.001, 0.025, 1.0, 2.5, 0.04, 0.08, 3., 4.5, 0.5, 2.0, 0.05, 0.200, 0.0, 0.5, 0.0, 1.0; break;
			} break;
		case FieldStrength::User:
			switch (m_nC) {
				case Signal::Components::One:   b.block(0, 0, nP - 1, 2).setZero(); break;
				case Signal::Components::Two:   b.block(0, 0, nP - 1, 2).setZero(); break;
				case Signal::Components::Three: b.block(0, 0, nP - 1, 2).setZero(); break;
			} break;
	}
	
	double minTR = numeric_limits<double>::max();
	for (auto &s : m_signals) {
		if (s->m_TR < minTR)
			minTR = s->m_TR;
	}
	b.block(nP - 1, 0, 1, 2) << -0.5 / minTR, 0.5 / minTR;
	
	return b;
}

const bool Model::validParameters(const VectorXd &params) const {
	// Negative T1/T2 makes no sense
	if ((params[0] <= 0.) || (params[1] <= 0.))
		return false;
	
	switch (m_nC) {
		case Signal::Components::One : return true;
		case Signal::Components::Two :
			// Check that T1_a, T2_a < T1_b, T2_b and that f_a makes sense
			if ((params[0] < params[2]) &&
				(params[1] < params[3]) &&
				(params[5] <= 1.0))
				return true;
			else
				return false;
		case Signal::Components::Three :
			// Check that T1/2_a < T1/2_b < T1/2_c and that f_a + f_c makes sense
			if ((params[0] < params[2]) &&
				(params[1] < params[3]) &&
				(params[2] < params[4]) &&
				(params[3] < params[5]) &&
				((params[7] + params[8]) <= 1.0))
				return true;
			else
				return false;
	}
}

ArrayXd Model::loadSignals(const vector<vector<double>> &sigSlices, const size_t voxelsPerSlice, const size_t vox) const {
	ArrayXd signal(size());
	size_t start = 0;
	for (size_t i = 0; i < m_signals.size(); i++) {
		ArrayXd thisSig(m_signals.at(i)->size());
		for (size_t j = 0; j < thisSig.rows(); j++) {
			thisSig(j) = sigSlices[i][voxelsPerSlice*j + vox];
		}
		if (m_scaling == Scaling::NormToMean)
			thisSig /= thisSig.mean();
		signal.segment(start, thisSig.rows()) = thisSig;
		start += thisSig.rows();
	}
	return signal;
}


SimpleModel::SimpleModel(const Signal::Components c, const Scaling s) : Model(c, s) {}
FiniteModel::FiniteModel(const Signal::Components c, const Scaling s) : Model(c, s) {}

void SimpleModel::parseSPGR(const size_t nFlip, const bool prompt) {
	double inTR = 0.;
	ArrayXd inAngles(nFlip);
	if (prompt) cout << "Enter TR (seconds): " << flush; cin >> inTR;
	if (prompt) cout << "Enter " << inAngles.size() << " Flip-angles (degrees): " << flush;
	for (int i = 0; i < inAngles.size(); i++)
		cin >> inAngles[i];
	string temp; getline(cin, temp); // Just to eat the newline
	m_signals.push_back(make_shared<SPGRSimple>(inAngles * M_PI / 180., inTR));
}

void FiniteModel::parseSPGR(const size_t nFlip, const bool prompt) {
	double inTR = 0., inTrf = 0., inTE = 0.;
	ArrayXd inAngles(nFlip);
	if (prompt) cout << "Enter TR (seconds): " << flush; cin >> inTR;
	if (prompt) cout << "Enter RF Pulse Length (seconds): " << flush; cin >> inTrf;
	if (prompt) cout << "Enter TE (seconds): " << flush; cin >> inTE;
	if (prompt) cout << "Enter " << inAngles.size() << " Flip-angles (degrees): " << flush;
	for (int i = 0; i < inAngles.size(); i++)
		cin >> inAngles[i];
	string temp; getline(cin, temp); // Just to eat the newline
	m_signals.push_back(make_shared<SPGRFinite>(inAngles * M_PI / 180., inTR, inTrf, inTE));
}

void SimpleModel::parseSSFP(const size_t nFlip, const size_t nPhases, const bool prompt) {
	double inTR = 0.;
	ArrayXd inPhases(nPhases), inAngles(nFlip);
	if (prompt) cout << "Enter " << nPhases << " phase-cycles (degrees): " << flush;
	for (size_t i = 0; i < nPhases; i++)
		cin >> inPhases(i);
	if (prompt) cout << "Enter TR (seconds): " << flush; cin >> inTR;
	if (prompt) cout << "Enter " << inAngles.size() << " Flip-angles (degrees): " << flush;
	for (ArrayXd::Index i = 0; i < inAngles.size(); i++) cin >>
		inAngles(i);
	string temp; getline(cin, temp); // Just to eat the newline
	m_signals.push_back(make_shared<SSFPSimple>(inAngles * M_PI / 180., inTR, inPhases * M_PI / 180.));
}

void FiniteModel::parseSSFP(const size_t nFlip, const size_t nPhases, const bool prompt) {
	double inTR = 0., inTrf = 0.;
	ArrayXd inPhases(nPhases), inAngles(nFlip);
	if (prompt) cout << "Enter " << nPhases << " phase-cycles (degrees): " << flush;
	for (size_t i = 0; i < nPhases; i++)
		cin >> inPhases(i);
	if (prompt) cout << "Enter TR (seconds): " << flush; cin >> inTR;
	if (prompt) cout << "Enter RF Pulse Length (seconds): " << flush; cin >> inTrf;
	if (prompt) cout << "Enter " << inAngles.size() << " Flip-angles (degrees): " << flush;
	for (ArrayXd::Index i = 0; i < inAngles.size(); i++) cin >>
		inAngles(i);
	string temp; getline(cin, temp); // Just to eat the newline
	m_signals.push_back(make_shared<SSFPFinite>(inAngles * M_PI / 180., inTR, inTrf, inPhases * M_PI / 180.));
}

#ifdef AGILENT
void SimpleModel::procparseSPGR(const Agilent::ProcPar &pp) {
	double inTR = 0.;
	ArrayXd inAngles = pp.realValues("flip1");
	inTR = pp.realValue("tr");
	inAngles = pp.realValues("flip1");
	m_signals.push_back(make_shared<SPGRSimple>(inAngles * M_PI / 180, inTR));
}

void FiniteModel::procparseSPGR(const Agilent::ProcPar &pp) {
	double inTR = 0., inTrf = 0., inTE = 0.;
	ArrayXd inAngles = pp.realValues("flip1");
	inTR = pp.realValue("tr");
	inAngles = pp.realValues("flip1");
	inTE = pp.realValue("te");
	inTrf = pp.realValue("p1") / 1.e6; // p1 is in microseconds
	m_signals.push_back(make_shared<SPGRFinite>(inAngles * M_PI / 180, inTR, inTrf, inTE));
}

void SimpleModel::procparseSSFP(const Agilent::ProcPar &pp) {
	double inTR = 0.;
	ArrayXd inPhases, inAngles;
	inPhases = pp.realValues("rfphase");
	inTR = pp.realValue("tr");
	inAngles = pp.realValues("flip1");
	m_signals.push_back(make_shared<SSFPSimple>(inAngles * M_PI / 180., inTR, inPhases * M_PI / 180.));
}

void FiniteModel::procparseSSFP(const Agilent::ProcPar &pp) {
	double inTR = 0., inTrf = 0.;
	ArrayXd inPhases, inAngles;
	inPhases = pp.realValues("rfphase");
	inTR = pp.realValue("tr");
	inAngles = pp.realValues("flip1");
	inTrf = pp.realValue("p1") / 1.e6; // p1 is in microseconds
	m_signals.push_back(make_shared<SSFPFinite>(inAngles * M_PI / 180., inTR, inTrf, inPhases * M_PI / 180.));
}
#endif //def AGILENT
