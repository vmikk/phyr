// -*- mode: C++; c-indent-level: 4; c-basic-offset: 4; indent-tabs-mode: nil; -*-

#include <RcppArmadillo.h>
#include <numeric>
#include <cmath>
#include <vector>

#include "cor_phylo.h"

using namespace Rcpp;


#define MAX_RETURN 10000000000






/*
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 
 Log likelihood function
 
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 */




//' Inline C++ that does most of the work related to the log-likelihood function.
//' 
//' See below for the wrapper around this function that replaces the many
//' arguments required here with one input `XPtr<LL_obj>` object.
//' 
//' 
//' @name cor_phylo_LL_
//' @noRd
//' 
//' 
inline double cor_phylo_LL_(const arma::vec& par,
                            const arma::mat& XX,
                            const arma::mat& UU,
                            const arma::mat& MM,
                            const arma::mat& Vphy,
                            const arma::mat& tau,
                            const bool& REML,
                            const bool& constrain_d,
                            const double& lower_d,
                            const bool& verbose,
                            const double& rcond_threshold) {
  
  
  uint_t n = Vphy.n_rows;
  uint_t p = XX.n_rows / n;
  
  arma::mat L = make_L(par, n, p);
  
  arma::mat R = L.t() * L;
  
  arma::vec d = make_d(par, n, p, constrain_d, lower_d, true);
  if (d.n_elem == 0) return MAX_RETURN;
  
  // OU transform
  arma::mat C = make_C(n, p, tau, d, Vphy, R);
  
  arma::mat V = make_V(C, MM);
  double rcond_dbl = arma::rcond(V);
  if (!arma::is_finite(rcond_dbl) || rcond_dbl < rcond_threshold) return MAX_RETURN;
  
  arma::mat iV = arma::inv(V);
  arma::mat denom = tp(UU) * iV * UU;
  rcond_dbl = arma::rcond(denom);
  if (!arma::is_finite(rcond_dbl) || rcond_dbl < rcond_threshold) return MAX_RETURN;
  
  arma::mat num = tp(UU) * iV * XX;
  arma::vec B0 = arma::solve(denom, num);
  arma::mat H = XX - UU * B0;
  
  double logdetV, det_sign;
  arma::log_det(logdetV, det_sign, iV);
  if (!arma::is_finite(logdetV)) return MAX_RETURN;
  logdetV *= -1;
  
  double LL;
  if (REML) {
    arma::mat to_det = tp(UU) * iV * UU;
    double det_val;
    arma::log_det(det_val, det_sign, to_det);
    double lhs = arma::as_scalar(tp(H) * iV * H);
    LL = 0.5 * (logdetV + det_val + lhs);
  } else {
    LL = 0.5 * arma::as_scalar(logdetV + tp(H) * iV * H);
  }
  
  if (verbose) {
    Rcout << LL << ' ';
    for (uint_t i = 0; i < par.n_elem; i++) Rcout << par(i) << ' ';
    Rcout << std::endl;
  }
  
  return LL;
}




//' `cor_phylo` log likelihood function.
//' 
//' 
//' @param par Initial values for the parameters to be optimized over.
//' @param ll_info_xptr `Rcpp::Xptr` object that points to a C++ `LogLikInfo` object.
//'     This object stores all the other information needed for the log likelihood
//'     function.
//' 
//' @noRd
//' 
//' @name cor_phylo_LL
//' 
//[[Rcpp::export]]
double cor_phylo_LL(const arma::vec& par,
                     const arma::mat& XX,
                     const arma::mat& UU,
                     const arma::mat& MM,
                     const arma::mat& Vphy,
                     const arma::mat& tau,
                     const bool& REML,
                     const bool& constrain_d,
                     const double& lower_d,
                     const bool& verbose,
                     const double& rcond_threshold) {
  
  double LL = cor_phylo_LL_(par, XX, UU, MM, Vphy, tau, REML,
                            constrain_d, lower_d, verbose, rcond_threshold);
  return LL;
}





//' Return reciprocal condition numbers for matrices in the log likelihood function.
//' 
//' This function is largely a repeat of the first part of the likelihood function.
//' It is used in the output to guide users wanting to change the `rcond_threshold`
//' argument.
//' 
//' 
//' @param par Initial values for the parameters to be optimized over.
//' @param ll_info A C++ `LogLikInfo` object storing all the information needed
//'     for the log likelihood function.
//' 
//' @noRd
//' 
//' @name return_rcond_vals
//' 
std::vector<double> return_rcond_vals(const LogLikInfo& ll_info) {
  
  const arma::vec& par(ll_info.min_par);
  const arma::mat& XX(ll_info.XX);
  const arma::mat& UU(ll_info.UU);
  const arma::mat& MM(ll_info.MM);
  const arma::mat& Vphy(ll_info.Vphy);
  const arma::mat& tau(ll_info.tau);
  // const bool& REML(ll_info.REML);
  const bool& constrain_d(ll_info.constrain_d);
  const double& lower_d(ll_info.lower_d);
  // const bool& verbose(ll_info.verbose);
  // const double& rcond_threshold(ll_info.rcond_threshold);
  
  std::vector<double> rconds_out(2);
  
  uint_t n = Vphy.n_rows;
  uint_t p = XX.n_rows / n;
  
  arma::mat L = make_L(par, n, p);
  
  arma::mat R = L.t() * L;
  
  arma::vec d = make_d(par, n, p, constrain_d, lower_d);

  // OU transform
  arma::mat C = make_C(n, p, tau, d, Vphy, R);
  
  arma::mat V = make_V(C, MM);
  double rcond_dbl = arma::rcond(V);
  rconds_out[0] = rcond_dbl;
  
  arma::mat iV = arma::inv(V);
  arma::mat denom = tp(UU) * iV * UU;
  rcond_dbl = arma::rcond(denom);
  rconds_out[1] = rcond_dbl;
  
  return rconds_out;
}

// Another version that uses XPtr; keeping it here in case I want to change back
// //[[Rcpp::export]]
// double cor_phylo_LL(const arma::vec& par,
//                     SEXP ll_info_xptr) {
//   
//   XPtr<LogLikInfo> ll_info(ll_info_xptr);
//   double LL = cor_phylo_LL_(par, ll_info->XX, ll_info->UU, ll_info->MM, 
//                             ll_info->Vphy, ll_info->tau, ll_info->REML, 
//                             ll_info->constrain_d, ll_info->verbose);
//   return LL;
// }
// The function below could also be useful to keep if you go back to using XPtr:
// // Combining standard_matrices with LogLikInfo::LogLikInfo to see what gets produced
// //[[Rcpp::export]]
// SEXP make_LL_info(const arma::mat& X,
//                   const std::vector<arma::mat>& U,
//                   const arma::mat& M,
//                   const arma::mat& Vphy_,
//                   const bool& REML_,
//                   const bool& constrain_d_,
//                   const bool& verbose_) {
//   
//   XPtr<LogLikInfo> ll_info_xptr(new LogLikInfo(X, U, M, Vphy_, REML_, constrain_d_,
//                                          verbose_));
//   
//   return ll_info_xptr;
// }
// 




/*
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 
 Fit using nlopt
 
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 */


//' Fit cor_phylo model using nlopt.
//'
//'
//' @inheritParams ll_info cp_get_output
//' @inheritParams max_iter cor_phylo
//' @inheritParams method cor_phylo
//' 
//' @return Nothing. `ll_info` is modified in place to have info from the model fit
//'   after this function is run.
//'
//' @name fit_cor_phylo_nlopt
//' @noRd
//' 
void fit_cor_phylo_nlopt(LogLikInfo& ll_info,
                         const double& rel_tol,
                         const int& max_iter,
                         const std::string& method) {
  
  Rcpp::Environment nloptr_pkg = Rcpp::Environment::namespace_env("nloptr");
  Rcpp::Function nloptr = nloptr_pkg["nloptr"];
  
  std::string nlopt_algor;
  
  if (method == "nelder-mead-nlopt") nlopt_algor = "NLOPT_LN_NELDERMEAD";
  if (method == "bobyqa") nlopt_algor = "NLOPT_LN_BOBYQA";
  if (method == "subplex") nlopt_algor = "NLOPT_LN_SBPLX";
  
  List options = List::create(_["algorithm"] = nlopt_algor,
                              _["ftol_rel"] = rel_tol,
                              _["ftol_abs"] = rel_tol,
                              _["xtol_rel"] = 0.0001,
                              _["maxeval"] = max_iter);
  
  List opt = nloptr(_["x0"] = ll_info.par0,
                   _["eval_f"] = Rcpp::InternalFunction(&cor_phylo_LL),
                   _["opts"] = options,
                   _["XX"] = ll_info.XX,
                   _["UU"] = ll_info.UU,
                   _["MM"] = ll_info.MM,
                   _["Vphy"] = ll_info.Vphy,
                   _["tau"] = ll_info.tau,
                   _["REML"] = ll_info.REML,
                   _["constrain_d"] = ll_info.constrain_d,
                   _["lower_d"] = ll_info.lower_d,
                   _["verbose"] = ll_info.verbose,
                   _["rcond_threshold"] = ll_info.rcond_threshold);
  
  ll_info.min_par = as<arma::vec>(opt["solution"]);
  
  ll_info.LL = as<double>(opt["objective"]);
  int convcode_ = as<int>(opt["status"]);
  
  if (convcode_ > 0) {
    if (convcode_ < 5) {
      ll_info.convcode = 0;
    } else {
      ll_info.convcode = 1;
    }
  } else {
    ll_info.convcode = -1 * convcode_ + 1;
  }

  ll_info.iters = as<arma::vec>(opt["iterations"])(0);
  
  if (ll_info.verbose) {
    Rcout << ll_info.LL << ' ';
    arma::vec& par(ll_info.min_par);
    for (uint_t i = 0; i < par.n_elem; i++) Rcout << par(i) << ' ';
    Rcout << std::endl;
  }
  
  return;
}



//' Fit `cor_phylo` model using R's `stats::optim`.
//' 
//' Make sure this doesn't get run in parallel!
//'
//'
//' @inheritParams ll_info_xptr cor_phylo_LL
//' @inheritParams max_iter cor_phylo
//' @inheritParams method cor_phylo
//' 
//' @return Nothing. `ll_info_xptr` is modified in place to have info from the model fit
//'   after this function is run.
//'
//' @name fit_cor_phylo_R
//' @noRd
//' 
void fit_cor_phylo_R(LogLikInfo& ll_info,
                     const double& rel_tol,
                     const int& max_iter,
                     const std::string& method,
                     const std::vector<double>& sann) {

  Rcpp::Environment stats = Rcpp::Environment::namespace_env("stats");
  Rcpp::Function optim = stats["optim"];
  
  Rcpp::List opt;
  
  if (method == "sann") {
    opt = optim(_["par"] = ll_info.par0,
                _["fn"] = Rcpp::InternalFunction(&cor_phylo_LL),
                _["method"] = "SANN",
                _["control"] = List::create(_["maxit"] = sann[0],
                                            _["temp"] = sann[1],
                                            _["tmax"] = sann[2],
                                            _["reltol"] = rel_tol),
                _["XX"] = ll_info.XX,
                _["UU"] = ll_info.UU,
                _["MM"] = ll_info.MM,
                _["Vphy"] = ll_info.Vphy,
                _["tau"] = ll_info.tau,
                _["REML"] = ll_info.REML,
                _["constrain_d"] = ll_info.constrain_d,
                _["lower_d"] = ll_info.lower_d,
                _["verbose"] = ll_info.verbose,
                _["rcond_threshold"] = ll_info.rcond_threshold);
    ll_info.par0 = as<arma::vec>(opt["par"]);
  }
  
  opt = optim(_["par"] = ll_info.par0,
              _["fn"] = Rcpp::InternalFunction(&cor_phylo_LL),
              _["method"] = "Nelder-Mead",
              _["control"] = List::create(_["maxit"] = max_iter,
                                          _["reltol"] = rel_tol),
              _["XX"] = ll_info.XX,
              _["UU"] = ll_info.UU,
              _["MM"] = ll_info.MM,
              _["Vphy"] = ll_info.Vphy,
              _["tau"] = ll_info.tau,
              _["REML"] = ll_info.REML,
              _["constrain_d"] = ll_info.constrain_d,
              _["lower_d"] = ll_info.lower_d,
              _["verbose"] = ll_info.verbose,
              _["rcond_threshold"] = ll_info.rcond_threshold);
  
  ll_info.min_par = as<arma::vec>(opt["par"]);
  
  ll_info.LL = as<double>(opt["value"]);
  ll_info.convcode = as<int>(opt["convergence"]);
  ll_info.iters = as<arma::vec>(opt["counts"])(0);
  
  if (ll_info.verbose) {
    Rcout << ll_info.LL << ' ';
    arma::vec& par(ll_info.min_par);
    for (uint_t i = 0; i < par.n_elem; i++) Rcout << par(i) << ' ';
    Rcout << std::endl;
  }
  
  return;
  
}



/*
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 
 Other functions
 
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 */


//' Standardize matrices in place.
//' 
//' Makes each column of the `X` matrix have mean of zero and standard deviation of 1.
//' If `U` isn't empty, this function makes each column in each matrix have
//' mean of zero and standard deviation of 1, unless all values are the same, in which
//' case it keeps the standard deviation at zero.
//' Divides each column of `M` by the original standard deviation of that column in 
//' `X`.
//' 
//' 
//' @inheritParams X cor_phylo_
//' @inheritParams U cor_phylo_
//' @inheritParams M cor_phylo_
//' 
//' @return Nothing. Matrices are standardized in place.
//' 
//' @name standardize_matrices
//' @noRd
//' 
void standardize_matrices(arma::mat& X,
                          std::vector<arma::mat>& U,
                          arma::mat& M) {
  
  uint_t p = X.n_cols;
  
  for (uint_t i = 0; i < p; i++) {
    double sd = arma::stddev(X.col(i));
    X.col(i) -= arma::mean(X.col(i));
    X.col(i) /= sd;
    M.col(i) /= sd;
  }
  
  for (uint_t i = 0; i < U.size(); i++) {
    arma::mat& Ui(U[i]);
    for (uint_t j = 0; j < Ui.n_cols; j++) {
      double sd = arma::stddev(Ui.col(j));
      Ui.col(j) -= arma::mean(Ui.col(j));
      if (sd > 0) Ui.col(j) /= sd;
    }
  }
  
  return;
}






//' Make an `LogLikInfo` object based on input matrices.
//' 
//' The output `LogLikInfo` is used for model fitting.
//' 
//' @inheritParams X cor_phylo_
//' @inheritParams U cor_phylo_
//' @inheritParams M cor_phylo_
//' @inheritParams Vphy_ cor_phylo_
//' @inheritParams REML_ cor_phylo_
//' @inheritParams constrain_d_ cor_phylo_
//' @inheritParams verbose_ cor_phylo_
//' 
//' @return a LogLikInfo that contains info necessary for model fitting
//' 
//' @name LogLikInfo
//' @noRd
//' 
LogLikInfo::LogLikInfo(const arma::mat& X,
                 const std::vector<arma::mat>& U,
                 const arma::mat& M,
                 const arma::mat& Vphy_,
                 const bool& REML_,
                 const bool& constrain_d_,
                 const double& lower_d_,
                 const bool& verbose_,
                 const double& rcond_threshold_) 
  : REML(REML_), constrain_d(constrain_d_), lower_d(lower_d_), verbose(verbose_), 
    rcond_threshold(rcond_threshold_), iters(0) {
  
  uint_t n = Vphy_.n_rows;
  uint_t p = X.n_cols;
  
  Vphy = Vphy_;
  Vphy /= Vphy_.max();
  double val, sign;
  arma::log_det(val, sign, Vphy);
  val = std::exp(val / n);
  Vphy /= val;
  
  tau = arma::vec(n, arma::fill::ones) * Vphy.diag().t() - Vphy;
  
  arma::mat Xs = X;
  std::vector<arma::mat> Us = U;
  arma::mat Ms = M;
  standardize_matrices(Xs, Us, Ms);
  
  
  XX = arma::reshape(Xs, Xs.n_elem, 1);
  MM = flex_pow(Ms, 2);
  MM.reshape(MM.n_elem, 1);
  UU = arma::kron(arma::eye<arma::mat>(p,p),
                  arma::mat(n, 1, arma::fill::ones));
  
  if (U.size() > 0) {
    arma::vec zeros(p, arma::fill::zeros);
    for (uint_t i = 0; i < p; i++) {
      arma::vec dd = zeros;
      dd[i] = 1;
      arma::mat u = arma::kron(dd, Us[i]);
      for (uint_t j = 0; j < u.n_cols; j++) {
        if (arma::diff(u.col(j)).max() > 0) {
          UU.insert_cols(UU.n_cols,1);
          UU.col(UU.n_cols-1) = u.col(j);
        }
      }
    }
  }
  
  arma::mat L;
  arma::mat eps = Xs;
  if (U.size() > 0) {
    for (uint_t i = 0; i < p; i++) {
      if (U[i].n_cols > 0) {
        const arma::mat& x(Us[i]);
        const arma::vec& y(Xs.col(i));
        arma::vec coef = arma::solve(x, y);
        arma::vec res = y - x * coef;
        eps.col(i) = res;
      } else {
        eps.col(i) = Xs.col(i) - arma::mean(Xs.col(i));
      }
    }
  }
  L = arma::cov(eps);
  safe_chol(L, "model fitting");
  L = tp(L);
  
  par0 = arma::vec((static_cast<double>(p) / 2) * (1 + p) + p);
  par0.fill(0.5);
  for (uint_t i = 0, j = 0, k = p - 1; i < p; i++) {
    par0(arma::span(j, k)) = L(arma::span(i, p-1), i);
    j = k + 1;
    k += (p - i - 1);
  }
  
  min_par = par0;
  
}



//' Make an `LogLikInfo` object based on input matrices and another LogLikInfo object.
//' 
//' The output `LogLikInfo` is used for model fitting.
//' 
//' *Note:* This version is used for bootstrapping.
//' It's different from the one above in that it doesn't re-normalize Vphy, UU, or tau.
//' If you normalize Vphy and tau twice (which would happen if I used the previous
//' version of this constructor), it can result in weird behavior.
//' Notably, the bootstrap replicate will sometimes not converge, but when I output the
//' same data and re-run cor_phylo on it, it'll converge.
//' This is confusing, so I'm trying to avoid that.
//' 
//' 
//' @inheritParams X cor_phylo_
//' @inheritParams U cor_phylo_
//' @inheritParams M cor_phylo_
//' @param other Another LogLikInfo object from which to derive much of the information.
//' 
//' @return a LogLikInfo that contains info necessary for model fitting
//' 
//' @name LogLikInfo
//' @noRd
//' 
LogLikInfo::LogLikInfo(const arma::mat& X,
                 const std::vector<arma::mat>& U,
                 const arma::mat& M,
                 const LogLikInfo& other) 
  : UU(other.UU), Vphy(other.Vphy), tau(other.tau),
    REML(other.REML), constrain_d(other.constrain_d), lower_d(other.lower_d),
    verbose(other.verbose), rcond_threshold(other.rcond_threshold), iters(0) {

  uint_t p = X.n_cols;
  
  arma::mat Xs = X;
  std::vector<arma::mat> Us = U;
  arma::mat Ms = M;
  standardize_matrices(Xs, Us, Ms);
  
  
  XX = arma::reshape(Xs, Xs.n_elem, 1);
  MM = flex_pow(Ms, 2);
  MM.reshape(MM.n_elem, 1);
  
  arma::mat L;
  arma::mat eps = Xs;
  if (U.size() > 0) {
    for (uint_t i = 0; i < p; i++) {
      if (U[i].n_cols > 0) {
        const arma::mat& x(Us[i]);
        const arma::vec& y(Xs.col(i));
        arma::vec coef = arma::solve(x, y);
        arma::vec res = y - x * coef;
        eps.col(i) = res;
      }
    }
  }
  L = arma::cov(eps);
  safe_chol(L, "a bootstrap replicate");
  L = tp(L);
  
  par0 = arma::vec((static_cast<double>(p) / 2) * (1 + p) + p);
  par0.fill(0.5);
  for (uint_t i = 0, j = 0, k = p - 1; i < p; i++) {
    par0(arma::span(j, k)) = L(arma::span(i, p-1), i);
    j = k + 1;
    k += (p - i - 1);
  }
  
  min_par = par0;
  
}




inline void main_output(arma::mat& corrs, arma::mat& B, arma::mat& B_cov, arma::vec& d,
                        const LogLikInfo& ll_info,
                        const arma::mat& X, const std::vector<arma::mat>& U) {
  
  uint_t n = X.n_rows;
  uint_t p = X.n_cols;
  
  arma::mat L = make_L(ll_info.min_par, n, p);
  
  arma::mat R = L.t() * L;
  
  corrs = make_corrs(R);
  
  d = make_d(ll_info.min_par, n, p, ll_info.constrain_d, ll_info.lower_d);
  
  // OU transform
  arma::mat C = make_C(n, p, ll_info.tau, d, ll_info.Vphy, R);
  
  arma::mat V = make_V(C, ll_info.MM);
  
  arma::mat iV = arma::inv(V);
  
  arma::mat denom = tp(ll_info.UU) * iV * ll_info.UU;
  
  arma::mat num = tp(ll_info.UU) * iV * ll_info.XX;
  
  arma::vec B0 = arma::solve(denom, num);
  
  make_B_B_cov(B, B_cov, B0, iV, ll_info.UU, X, U);
  
  return;
}


//' Retrieve objects for output `cor_phylo` object.
//' 
//' @inheritParams X cor_phylo_
//' @inheritParams U cor_phylo_
//' @param ll_info an LogLikInfo object that contains info necessary to fit the model.
//'   After optimization, it contains info from the model fit.
//' 
//' @return a list containing output information, to later be coerced to a `cor_phylo`
//'   object by the `cor_phylo` function.
//' 
//' @name cp_get_output
//' @noRd
//' 
List cp_get_output(const arma::mat& X,
                   const std::vector<arma::mat>& U,
                   const arma::mat& M,
                   LogLikInfo& ll_info,
                   const double& rel_tol,
                   const int& max_iter,
                   const std::string& method,
                   const uint_t& boot,
                   const std::string& keep_boots,
                   const std::vector<double>& sann) {

  
  uint_t n = X.n_rows;
  uint_t p = X.n_cols;
  
  /*
   Get the main output from cor_phylo: correlations, coefficient estimates, 
   coefficient covariances, and phylogenetic signals.
   The objects constructed below correspond to this information.
   */
  arma::mat corrs;
  arma::mat B;
  arma::mat B_cov;
  arma::vec d;
  main_output(corrs, B, B_cov, d, ll_info, X, U);
  
  double logLik = -0.5 * std::log(2 * arma::datum::pi);
  if (ll_info.REML) {
    logLik *= (n * p - ll_info.UU.n_cols);
    arma::mat to_det = tp(ll_info.XX) * ll_info.XX;
    double det_val, det_sign;
    arma::log_det(det_val, det_sign, to_det);
    logLik += 0.5 * det_val - ll_info.LL;
  } else {
    logLik *= (n * p);
    logLik -= ll_info.LL;
  }
  
  double k = ll_info.min_par.n_elem + ll_info.UU.n_cols;
  double AIC, BIC;
  AIC = -2 * logLik + 2 * k;
  BIC = -2 * logLik + k * std::log(n / arma::datum::pi);
  
  std::vector<double> rcond_vals = return_rcond_vals(ll_info);
  
  List boot_list = List::create();
  if (boot > 0) {
    // `BootMats` stores matrices that we'll need for bootstrapping
    BootMats bm(X, U, M, B, d, ll_info);
    BootResults br(p, B.n_rows, boot);
    for (uint_t b = 0; b < boot; b++) {
      Rcpp::checkUserInterrupt();
      bm.one_boot(ll_info, br, b, rel_tol, max_iter, method, keep_boots, sann);
    }
    std::vector<NumericMatrix> boot_out_mats(br.out_inds.size());
    for (uint_t i = 0; i < br.out_inds.size(); i++) {
      boot_out_mats[i] = wrap(br.out_mats[i]);
    }
    boot_list = List::create(_["corrs"] = br.corrs, _["d"] = br.d,
                             _["B0"] = br.B0, _["B_cov"] = br.B_cov,
                             _["inds"] = br.out_inds,
                             _["convcodes"] = br.out_codes,
                             _["mats"] = boot_out_mats);
  }
  
  // Now the final output list
  List out = List::create(
    _["corrs"] = corrs,
    _["d"] = d,
    _["B"] = B,
    _["B_cov"] = B_cov,
    _["logLik"] = logLik,
    _["AIC"] = AIC,
    _["BIC"] = BIC,
    _["niter"] = ll_info.iters,
    _["convcode"] = ll_info.convcode,
    _["rcond_vals"] = rcond_vals,
    _["bootstrap"] = boot_list
  );
  
  return out;
}



//' Inner function to create necessary matrices and do model fitting.
//' 
//' @param X a n x p matrix with p columns containing the values for the n taxa.
//' @param U a list of p matrices corresponding to the p columns of `X`, with each 
//'   matrix containing independent variables for the corresponding column of `X`.
//' @param M a n x p matrix with p columns containing standard errors of the trait 
//'   values in `X`. 
//' @param Vphy_ phylogenetic variance-covariance matrix from the input phylogeny.
//' @inheritParams REML cor_phylo
//' @inheritParams constrain_d cor_phylo
//' @inheritParams verbose cor_phylo
//' @inheritParams max_iter cor_phylo
//' @param method the `method` input to `cor_phylo`.
//' 
//' @return a list containing output information, to later be coerced to a `cor_phylo`
//'   object by the `cor_phylo` function.
//' @noRd
//' @name cor_phylo_
//' 
//[[Rcpp::export]]
List cor_phylo_(const arma::mat& X,
                const std::vector<arma::mat>& U,
                const arma::mat& M,
                const arma::mat& Vphy_,
                const bool& REML,
                const bool& constrain_d,
                const double& lower_d,
                const bool& verbose,
                const double& rcond_threshold,
                const double& rel_tol,
                const int& max_iter,
                const std::string& method,
                const uint_fast32_t& boot,
                const std::string& keep_boots,
                const std::vector<double>& sann) {
  

  // LogLikInfo is C++ class to use for organizing info for optimizing
  LogLikInfo ll_info(X, U, M, Vphy_, REML, constrain_d, lower_d, verbose, rcond_threshold);

  /*
   Do the fitting.
   Methods "nelder-mead-r" and "sann" use R's `stats::optim`.
   Otherwise, use nlopt.
   */
  if (method == "nelder-mead-r" || method == "sann") {
    fit_cor_phylo_R(ll_info, rel_tol, max_iter, method, sann);
  } else {
    fit_cor_phylo_nlopt(ll_info, rel_tol, max_iter, method);
  }
  
  // Retrieve output from `ll_info` object and convert to list
  List output = cp_get_output(X, U, M, ll_info, rel_tol, max_iter, method,
                              boot, keep_boots, sann);
  
  return output;
  
}










/*
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 
 Bootstrapping functions
 
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 ***************************************************************************************
 */



BootMats::BootMats(const arma::mat& X_, const std::vector<arma::mat>& U_,
                     const arma::mat& M_,
                     const arma::mat& B_, const arma::vec& d_, const LogLikInfo& ll_info)
  : X(X_), U(U_), M(M_), X_new(), iD(), X_pred() {
  
  uint_t n = ll_info.Vphy.n_rows;
  uint_t p = X.n_cols;
  
  arma::mat L = make_L(ll_info.min_par, n, p);
  arma::mat R = L.t() * L;
  arma::mat C = make_C(n, p, ll_info.tau, d_, ll_info.Vphy, R);
  arma::mat V = make_V(C, ll_info.MM);
  iD = V;
  safe_chol(iD, "bootstrapping-matrices setup");
  iD = iD.t();
  
  // For predicted X values (i.e., without error)
  X_pred = ll_info.UU;
  X_pred = tp(arma::conv_to<arma::vec>::from(B_.col(0))) * tp(X_pred);
  X_pred = tp(X_pred);
  X_pred.reshape(n, p);
  
  return;
}

//' Iterate from a BootMats object in prep for a bootstrap replicate.
//' 
//' This ultimately updates the LogLikInfo object with new XX and MM matrices,
//' and updates the BootResults object with the mean and sd.
//' 
//' @param ll_info An LogLikInfo object that will inform the next call to the
//'     log-likelihood function.
//' @param br A BootResults object that stores output from bootstrapping.
//' 
//' @name boot_mats_iterate
//' @noRd
//' 
LogLikInfo BootMats::iterate(const LogLikInfo& ll_info) {

  uint_t n = X.n_rows;
  uint_t p = X.n_cols;
  
  X_new = X_pred;
  
  arma::mat X_rnd = iD * as<arma::vec>(rnorm(n * p));
  X_rnd.reshape(n, p);
  
  for (uint_t i = 0; i < p; i++) {
    double sd_ = arma::stddev(X.col(i));
    X_new.col(i) += (X_rnd.col(i) * sd_);
  }
  // X_new = X_pred + X_rnd;

  LogLikInfo ll_info_new(X_new, U, M, ll_info);

  return ll_info_new;
}



// Method to return bootstrapped data
void BootMats::boot_data(LogLikInfo& ll_info, BootResults& br, const uint_t& i) {
  
  br.out_inds.push_back(i+1);
  br.out_codes.push_back(ll_info.convcode);
  
  br.out_mats.push_back(X_new);
  
  return;
}

void BootMats::one_boot(const LogLikInfo& ll_info, BootResults& br,
                        const uint_t& i, const double& rel_tol, const int& max_iter,
                        const std::string& method, const std::string& keep_boots,
                        const std::vector<double>& sann) {
  
  // Generate new data
  LogLikInfo new_ll_info = iterate(ll_info);
  
  /*
   Do the fitting.
   Methods "nelder-mead-r" and "sann" use R's `stats::optim`.
   Otherwise, use nlopt.
   */
  if (method == "nelder-mead-r" || method == "sann") {
    // Do the fitting:
    fit_cor_phylo_R(new_ll_info, rel_tol, max_iter, method, sann);
  } else {
    // Same thing for nlopt version
    fit_cor_phylo_nlopt(new_ll_info, rel_tol, max_iter, method);
  }
  // Determine whether convergence failed:
  bool failed = new_ll_info.convcode != 0;
  
  if (keep_boots == "all" || (keep_boots == "fail" && failed)) {
    boot_data(new_ll_info, br, i);
  }

  arma::mat corrs;
  arma::mat B;
  arma::mat B_cov;
  arma::vec d;
  main_output(corrs, B, B_cov, d, new_ll_info, X_new, U);

  // Add values to BootResults
  br.insert_values(i, corrs, B.col(0), B_cov, d);
  
  return;
}

