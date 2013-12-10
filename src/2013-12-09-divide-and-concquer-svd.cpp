// -*- mode: C++; c-indent-level: 4; c-basic-offset: 4; indent-tabs-mode: nil; -*-
/**
 * @title Performance of the divide-and-conquer SVD algorithm
 * @author Dirk Eddelbuettel
 * @license GPL (>= 2)
 * @tags armadillo matrix
 * @summary This example illustrates the performance gain from divide-and-conquer SVD
 *
 * The ubiquitous [LAPACK](http://www.netlib.org/lapack) library provides several 
 * implementations for the singular-value decomposition (SVD). We will illustrate 
 * possible speed gains from using the divide-and-conquer method by comparing it
 * to the base case.
 */

#include <RcppArmadillo.h>

// [[Rcpp::depends(RcppArmadillo)]]

// [[Rcpp::export]]
arma::vec baseSVD(const arma::mat & X) {
    arma::mat U, V;
    arma::vec S;
    arma::svd(U, S, V, X, "standard");
    return S;
}

// [[Rcpp::export]]
arma::vec dcSVD(const arma::mat & X) {
    arma::mat U, V;
    arma::vec S;
    arma::svd(U, S, V, X, "dc");
    return S;
}

/**
 * Having the two implementations, which differ only in the `method`
 * argument (added recently in Armadillo 3.930), we are ready to do a
 * simple timing comparison.
 */

/*** R
library(microbenchmark)
set.seed(42)
X <- matrix(rnorm(16e4), 4e2, 4e2)
microbenchmark(baseSVD(X), dcSVD(X))
*/

/**
 * The speed gain is noticeable which a ratio of about 3.9 at the
 * median. However, we can also look at complex-valued matrices.
 */

// [[Rcpp::export]]
arma::vec cxBaseSVD(const arma::cx_mat & X) {
    arma::cx_mat U, V;
    arma::vec S;
    arma::svd(U, S, V, X, "standard");
    return S;
}

// [[Rcpp::export]]
arma::vec cxDcSVD(const arma::cx_mat & X) {
    arma::cx_mat U, V;
    arma::vec S;
    arma::svd(U, S, V, X, "dc");
    return S;
}

/*** R
A <- matrix(rnorm(16e4), 4e2, 4e2)
B <- matrix(rnorm(16e4), 4e2, 4e2)
X <- A + 1i * B
microbenchmark(cxBaseSVD(X), cxDcSVD(X))
 */

/**
 * Here the difference is even more pronounced at about 4.8. However,
 * it is precisely this complex-value divide-and-conquer method which
 * is missing in R's own Lapack. So R built with the default
 * configuration can not currently take advantage of the
 * complex-valued divide-and-conquer algorithm. Only builds which use
 * an external Lapack library (as for example the Debian and Ubuntu
 * builds) can. Let's hope that R will add this functionality to its
 * next release R 3.1.0. <em>Update: And the underlying `zgesdd`
 * routine has now been added to the upcoming R 3.1.0 release. Nice.</em>
 */
